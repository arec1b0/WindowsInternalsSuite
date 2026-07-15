#pragma once

// View model for the Process Explorer tab.
//
// Holds a flat row list plus a parent/child tree derived from it, and computes
// CPU usage by differencing consecutive snapshots. Win32-free by design.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/process/IProcessManager.hpp"
#include "ui/interfaces/IViewModel.hpp"

namespace wis::ui {

// One row of the process list, pre-formatted where formatting is OS-independent.
struct ProcessRow {
    std::uint32_t pid = 0;
    std::uint32_t parentPid = 0;
    std::wstring imageName;
    std::uint32_t sessionId = 0;
    std::uint32_t threadCount = 0;
    std::uint32_t handleCount = 0;

    double cpuPercent = 0.0;         // 0.0 on the first refresh (no baseline yet)
    std::string cpuText;             // e.g. "12.4" or "-" when unmeasured
    std::uint64_t workingSetBytes = 0;
    std::string workingSetText;      // e.g. "184.2 MB"
    std::uint64_t privateBytes = 0;
    std::string privateText;

    // Raw FILETIME (100 ns since 1601-01-01 UTC). Views format this, because a
    // readable timestamp needs OS calls that must not leak into a view model.
    std::uint64_t createTime = 0;
};

// Tree edge set: for each PID, the PIDs of its verified children.
struct ProcessTree {
    std::vector<std::uint32_t> roots;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> children;
};

// Details fetched on demand for the selected process.
struct ProcessDetailsRow {
    bool loaded = false;
    std::wstring fullImagePath;
    std::wstring commandLine;
    std::string architecture;   // "x64", "x86", "ARM64", "unknown"
};

class ProcessExplorerVM final : public IViewModel {
public:
    // `manager` must outlive this view model. `processorCount` scales the CPU
    // percentage; pass the logical processor count reported by SystemInfo.
    ProcessExplorerVM(const core::IProcessManager& manager, std::uint32_t processorCount);

    // Refreshes using the steady clock as the sampling reference.
    void refresh() override;

    // Test seam: refreshes with an explicit sampling instant, so CPU deltas can
    // be exercised deterministically without waiting on a real clock.
    void refreshAt(std::chrono::steady_clock::time_point now);

    [[nodiscard]] const std::vector<ProcessRow>& rows() const noexcept { return rows_; }
    [[nodiscard]] const ProcessTree& tree() const noexcept { return tree_; }

    // Selection. Selecting a PID does not fetch details; call loadDetails().
    void select(std::uint32_t pid);
    void clearSelection();
    [[nodiscard]] std::optional<std::uint32_t> selectedPid() const noexcept {
        return selectedPid_;
    }

    // Fetches path/command line/architecture for the current selection. This
    // opens the process and may be denied, which is reported via lastError().
    void loadDetails();
    [[nodiscard]] const ProcessDetailsRow& details() const noexcept { return details_; }

    // Row lookup by PID, or nullptr when the PID is not in the current snapshot.
    [[nodiscard]] const ProcessRow* findRow(std::uint32_t pid) const;

private:
    // Per-PID CPU baseline carried between refreshes.
    struct CpuSample {
        std::uint64_t cpuTime100ns = 0;  // kernel + user
        std::uint64_t createTime = 0;    // guards against PID reuse
    };

    void rebuildTree();

    const core::IProcessManager& manager_;
    std::uint32_t processorCount_ = 1;

    std::vector<ProcessRow> rows_;
    ProcessTree tree_;
    std::unordered_map<std::uint32_t, CpuSample> previousSamples_;
    std::optional<std::chrono::steady_clock::time_point> previousSampleTime_;

    std::optional<std::uint32_t> selectedPid_;
    ProcessDetailsRow details_;
};

}  // namespace wis::ui
