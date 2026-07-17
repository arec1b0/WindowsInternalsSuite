#pragma once

// View model for the Thread Explorer tab. Lists the threads of one process and
// computes per-thread CPU usage by differencing snapshots. Win32-free.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/symbols/ISymbolResolver.hpp"
#include "core/thread/IThreadManager.hpp"
#include "ui/detail/CpuSampler.hpp"
#include "ui/interfaces/IViewModel.hpp"

namespace wis::ui {

struct ThreadRow {
    std::uint32_t tid = 0;
    std::string state;              // "Running", "Waiting", ...
    std::int32_t priority = 0;
    std::int32_t basePriority = 0;

    std::uint64_t startAddress = 0;
    std::string startAddressText;   // symbolized when a resolver is available

    double cpuPercent = 0.0;
    std::string cpuText;            // "-" until a baseline exists
    std::uint64_t cpuTime100ns = 0;
    std::string cpuTimeText;        // "h:mm:ss.mmm"
    std::uint32_t contextSwitches = 0;

    // Raw FILETIME; views format it (see ProcessRow::createTime).
    std::uint64_t createTime = 0;
};

// Details fetched on demand for the selected thread.
struct ThreadDetailsRow {
    bool loaded = false;
    std::uint64_t win32StartAddress = 0;
    std::string win32StartAddressText;
    std::uint64_t tebBaseAddress = 0;
    std::string tebBaseAddressText;
};

class ThreadExplorerVM final : public IViewModel {
public:
    // `manager` must outlive this view model. `resolver` is optional: when null,
    // addresses are shown numerically instead of symbolized. `processorCount`
    // scales the CPU percentage.
    ThreadExplorerVM(const core::IThreadManager& manager, std::uint32_t processorCount,
                     const core::ISymbolResolver* resolver = nullptr);

    // Binds the view model to a process. Clears CPU history, because thread ids
    // from a different process share no baseline with the previous subject.
    void setProcess(std::uint32_t pid);
    [[nodiscard]] std::optional<std::uint32_t> processId() const noexcept { return pid_; }

    void refresh() override;

    // Test seam: refresh with an explicit sampling instant.
    void refreshAt(std::chrono::steady_clock::time_point now);

    [[nodiscard]] const std::vector<ThreadRow>& rows() const noexcept { return rows_; }

    void select(std::uint32_t tid);
    void clearSelection();
    [[nodiscard]] std::optional<std::uint32_t> selectedTid() const noexcept {
        return selectedTid_;
    }

    // Opens the selected thread to read its Win32 start address and TEB base.
    void loadDetails();
    [[nodiscard]] const ThreadDetailsRow& details() const noexcept { return details_; }

    [[nodiscard]] const ThreadRow* findRow(std::uint32_t tid) const;

private:
    // Symbolizes an address, or formats it numerically when no resolver is bound.
    [[nodiscard]] std::string describeAddress(std::uint64_t address) const;

    const core::IThreadManager& manager_;
    const core::ISymbolResolver* resolver_ = nullptr;
    detail::CpuSampler cpuSampler_;

    std::optional<std::uint32_t> pid_;
    std::vector<ThreadRow> rows_;

    std::optional<std::uint32_t> selectedTid_;
    ThreadDetailsRow details_;
};

}  // namespace wis::ui
