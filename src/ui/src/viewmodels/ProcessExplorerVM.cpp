#include "ui/viewmodels/ProcessExplorerVM.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_set>
#include <utility>

namespace wis::ui {
namespace {

// Formats a byte count with a binary-prefix unit, e.g. "184.2 MB".
std::string formatBytes(std::uint64_t bytes) {
    constexpr const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    constexpr std::size_t unitCount = sizeof(units) / sizeof(units[0]);

    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < unitCount) {
        value /= 1024.0;
        ++unit;
    }

    char buffer[64] = {};
    // Whole bytes read better without a fractional part.
    if (unit == 0) {
        std::snprintf(buffer, sizeof(buffer), "%llu B",
                      static_cast<unsigned long long>(bytes));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);
    }
    return std::string(buffer);
}

std::string formatPercent(double percent) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f", percent);
    return std::string(buffer);
}

}  // namespace

ProcessExplorerVM::ProcessExplorerVM(const core::IProcessManager& manager,
                                     std::uint32_t processorCount)
    : manager_(manager), processorCount_(processorCount > 0 ? processorCount : 1) {}

void ProcessExplorerVM::refresh() { refreshAt(std::chrono::steady_clock::now()); }

void ProcessExplorerVM::refreshAt(std::chrono::steady_clock::time_point now) {
    auto snapshotResult = manager_.snapshot();
    if (!snapshotResult) {
        setError(snapshotResult.error().message());
        notifyChanged();
        return;
    }
    clearError();

    const std::vector<core::ProcessInfo>& processes = snapshotResult.value();

    // Elapsed wall time since the previous sample, in 100 ns units, matching the
    // unit of the kernel/user time counters.
    std::uint64_t elapsed100ns = 0;
    if (previousSampleTime_) {
        const auto delta =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - *previousSampleTime_);
        elapsed100ns = static_cast<std::uint64_t>(delta.count() / 100);
    }

    std::unordered_map<std::uint32_t, CpuSample> currentSamples;
    currentSamples.reserve(processes.size());

    std::vector<ProcessRow> rows;
    rows.reserve(processes.size());

    for (const core::ProcessInfo& process : processes) {
        ProcessRow row;
        row.pid = process.pid;
        row.parentPid = process.parentPid;
        row.imageName = process.imageName;
        row.sessionId = process.sessionId;
        row.threadCount = process.threadCount;
        row.handleCount = process.handleCount;
        row.createTime = process.createTime;

        row.workingSetBytes = process.workingSetBytes;
        row.workingSetText = formatBytes(process.workingSetBytes);
        row.privateBytes = process.privateBytes;
        row.privateText = formatBytes(process.privateBytes);

        const std::uint64_t cpuTime = process.kernelTime + process.userTime;
        currentSamples.emplace(process.pid, CpuSample{cpuTime, process.createTime});

        // CPU usage needs a prior sample for the same process instance. A PID
        // whose createTime changed is a different process that reused the id,
        // so its old baseline must not be applied.
        row.cpuText = "-";
        if (elapsed100ns > 0) {
            const auto previous = previousSamples_.find(process.pid);
            if (previous != previousSamples_.end() &&
                previous->second.createTime == process.createTime &&
                cpuTime >= previous->second.cpuTime100ns) {
                const std::uint64_t cpuDelta = cpuTime - previous->second.cpuTime100ns;
                const double capacity =
                    static_cast<double>(elapsed100ns) * static_cast<double>(processorCount_);
                double percent = (capacity > 0.0)
                                     ? (static_cast<double>(cpuDelta) / capacity) * 100.0
                                     : 0.0;
                // Scheduling jitter between the two samples can push the ratio a
                // hair over 100; clamp rather than display an impossible number.
                percent = std::clamp(percent, 0.0, 100.0);
                row.cpuPercent = percent;
                row.cpuText = formatPercent(percent);
            }
        }

        rows.push_back(std::move(row));
    }

    rows_ = std::move(rows);
    previousSamples_ = std::move(currentSamples);
    previousSampleTime_ = now;

    rebuildTree();

    // A selection that vanished from the snapshot is stale; drop it so the view
    // does not keep showing details for a process that exited.
    if (selectedPid_ && findRow(*selectedPid_) == nullptr) {
        clearSelection();
    }

    notifyChanged();
}

void ProcessExplorerVM::rebuildTree() {
    tree_.roots.clear();
    tree_.children.clear();

    // Index rows by PID for parent verification.
    std::unordered_map<std::uint32_t, const ProcessRow*> byPid;
    byPid.reserve(rows_.size());
    for (const ProcessRow& row : rows_) {
        byPid.emplace(row.pid, &row);
    }

    for (const ProcessRow& row : rows_) {
        // A process is a child only when its recorded parent still exists AND
        // that parent started no later than the child. Windows reuses PIDs, so a
        // PPID can point at an unrelated process that happens to hold the id
        // now; the create-time ordering rules those out.
        const auto parent = byPid.find(row.parentPid);
        const bool parentIsReal = row.parentPid != 0 && row.parentPid != row.pid &&
                                  parent != byPid.end() &&
                                  parent->second->createTime <= row.createTime;

        if (parentIsReal) {
            tree_.children[row.parentPid].push_back(row.pid);
        } else {
            tree_.roots.push_back(row.pid);
        }
    }

    // Stable ordering keeps the tree from reshuffling between refreshes.
    std::sort(tree_.roots.begin(), tree_.roots.end());
    for (auto& entry : tree_.children) {
        std::sort(entry.second.begin(), entry.second.end());
    }
}

void ProcessExplorerVM::select(std::uint32_t pid) {
    if (selectedPid_ && *selectedPid_ == pid) {
        return;
    }
    selectedPid_ = pid;
    details_ = ProcessDetailsRow{};  // details are stale for a new selection
    notifyChanged();
}

void ProcessExplorerVM::clearSelection() {
    selectedPid_.reset();
    details_ = ProcessDetailsRow{};
    notifyChanged();
}

void ProcessExplorerVM::loadDetails() {
    if (!selectedPid_) {
        return;
    }

    auto detailsResult = manager_.queryDetails(*selectedPid_);
    if (!detailsResult) {
        // Access denial on protected processes is expected, not exceptional.
        details_ = ProcessDetailsRow{};
        setError(detailsResult.error().message());
        notifyChanged();
        return;
    }
    clearError();

    const core::ProcessDetails& source = detailsResult.value();
    details_.loaded = true;
    details_.fullImagePath = source.fullImagePath;
    details_.commandLine = source.commandLine;
    details_.architecture = std::string(core::toString(source.architecture));

    notifyChanged();
}

const ProcessRow* ProcessExplorerVM::findRow(std::uint32_t pid) const {
    const auto it = std::find_if(rows_.begin(), rows_.end(),
                                 [pid](const ProcessRow& row) { return row.pid == pid; });
    return (it != rows_.end()) ? &(*it) : nullptr;
}

}  // namespace wis::ui
