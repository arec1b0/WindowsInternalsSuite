#include "ui/viewmodels/ThreadExplorerVM.hpp"

#include <algorithm>
#include <utility>

#include "ui/detail/Format.hpp"

namespace wis::ui {

ThreadExplorerVM::ThreadExplorerVM(const core::IThreadManager& manager,
                                   std::uint32_t processorCount,
                                   const core::ISymbolResolver* resolver)
    : manager_(manager), resolver_(resolver), cpuSampler_(processorCount) {}

void ThreadExplorerVM::setProcess(std::uint32_t pid) {
    if (pid_ && *pid_ == pid) {
        return;
    }
    pid_ = pid;

    // Thread ids are only meaningful within their process, and the sampler keys
    // on raw ids; carrying history across a subject change would let a tid from
    // the old process seed a bogus delta for an unrelated thread here.
    cpuSampler_.reset();
    rows_.clear();
    clearSelection();
    notifyChanged();
}

void ThreadExplorerVM::refresh() { refreshAt(std::chrono::steady_clock::now()); }

void ThreadExplorerVM::refreshAt(std::chrono::steady_clock::time_point now) {
    if (!pid_) {
        rows_.clear();
        notifyChanged();
        return;
    }

    auto snapshotResult = manager_.snapshot(*pid_);
    if (!snapshotResult) {
        setError(snapshotResult.error().message());
        rows_.clear();
        notifyChanged();
        return;
    }
    clearError();

    const std::vector<core::ThreadInfo>& threads = snapshotResult.value();
    cpuSampler_.beginRound(now);

    std::vector<ThreadRow> rows;
    rows.reserve(threads.size());

    for (const core::ThreadInfo& thread : threads) {
        ThreadRow row;
        row.tid = thread.tid;
        row.state = std::string(core::toString(thread.state));
        row.priority = thread.priority;
        row.basePriority = thread.basePriority;
        row.contextSwitches = thread.contextSwitches;
        row.createTime = thread.createTime;

        row.startAddress = thread.startAddress;
        row.startAddressText = describeAddress(thread.startAddress);

        row.cpuTime100ns = thread.kernelTime + thread.userTime;
        row.cpuTimeText = detail::formatCpuTime(row.cpuTime100ns);

        const detail::CpuUsage usage =
            cpuSampler_.update(thread.tid, row.cpuTime100ns, thread.createTime);
        row.cpuPercent = usage.percent;
        row.cpuText = usage.measured ? detail::formatPercent(usage.percent) : "-";

        rows.push_back(std::move(row));
    }

    cpuSampler_.endRound();
    rows_ = std::move(rows);

    // Drop a selection whose thread has exited.
    if (selectedTid_ && findRow(*selectedTid_) == nullptr) {
        clearSelection();
    }

    notifyChanged();
}

std::string ThreadExplorerVM::describeAddress(std::uint64_t address) const {
    if (address == 0) {
        return "-";
    }
    if (resolver_ != nullptr && resolver_->ready()) {
        const core::SymbolLocation location = resolver_->resolve(address);
        return core::formatSymbol(location, address);
    }
    return detail::formatAddress(address);
}

void ThreadExplorerVM::select(std::uint32_t tid) {
    if (selectedTid_ && *selectedTid_ == tid) {
        return;
    }
    selectedTid_ = tid;
    details_ = ThreadDetailsRow{};
    notifyChanged();
}

void ThreadExplorerVM::clearSelection() {
    selectedTid_.reset();
    details_ = ThreadDetailsRow{};
    notifyChanged();
}

void ThreadExplorerVM::loadDetails() {
    if (!selectedTid_) {
        return;
    }

    auto detailsResult = manager_.queryDetails(*selectedTid_);
    if (!detailsResult) {
        details_ = ThreadDetailsRow{};
        setError(detailsResult.error().message());
        notifyChanged();
        return;
    }
    clearError();

    const core::ThreadDetails& source = detailsResult.value();
    details_.loaded = true;
    details_.win32StartAddress = source.win32StartAddress;
    details_.win32StartAddressText = describeAddress(source.win32StartAddress);
    details_.tebBaseAddress = source.tebBaseAddress;
    details_.tebBaseAddressText = (source.tebBaseAddress != 0)
                                      ? detail::formatAddress(source.tebBaseAddress)
                                      : std::string("-");

    notifyChanged();
}

const ThreadRow* ThreadExplorerVM::findRow(std::uint32_t tid) const {
    const auto it = std::find_if(rows_.begin(), rows_.end(),
                                 [tid](const ThreadRow& row) { return row.tid == tid; });
    return (it != rows_.end()) ? &(*it) : nullptr;
}

}  // namespace wis::ui
