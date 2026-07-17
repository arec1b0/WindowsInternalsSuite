#include "ui/viewmodels/ProcessExplorerVM.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "ui/detail/Format.hpp"

namespace wis::ui {

ProcessExplorerVM::ProcessExplorerVM(const core::IProcessManager& manager,
                                     std::uint32_t processorCount)
    : manager_(manager), cpuSampler_(processorCount) {}

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
    cpuSampler_.beginRound(now);

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
        row.workingSetText = detail::formatBytes(process.workingSetBytes);
        row.privateBytes = process.privateBytes;
        row.privateText = detail::formatBytes(process.privateBytes);

        const detail::CpuUsage usage = cpuSampler_.update(
            process.pid, process.kernelTime + process.userTime, process.createTime);
        row.cpuPercent = usage.percent;
        row.cpuText = usage.measured ? detail::formatPercent(usage.percent) : "-";

        rows.push_back(std::move(row));
    }

    cpuSampler_.endRound();
    rows_ = std::move(rows);

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
