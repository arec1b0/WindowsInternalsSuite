#pragma once

// Domain model for a thread. Two tiers, mirroring ProcessInfo:
//   ThreadInfo    - cheap fields from the process snapshot's inline thread array.
//   ThreadDetails - fields requiring the thread to be opened (best-effort).

#include <cstdint>
#include <string_view>

namespace wis::core {

// Thread scheduling state (KTHREAD_STATE). Values match the kernel enumeration.
enum class ThreadState : std::uint8_t {
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting,
    Transition,
    DeferredReady,
    GateWaitObsolete,
    WaitingForProcessInSwap,
    Unknown,
};

[[nodiscard]] constexpr std::string_view toString(ThreadState state) noexcept {
    switch (state) {
        case ThreadState::Initialized:             return "Initialized";
        case ThreadState::Ready:                   return "Ready";
        case ThreadState::Running:                 return "Running";
        case ThreadState::Standby:                 return "Standby";
        case ThreadState::Terminated:              return "Terminated";
        case ThreadState::Waiting:                 return "Waiting";
        case ThreadState::Transition:              return "Transition";
        case ThreadState::DeferredReady:           return "DeferredReady";
        case ThreadState::GateWaitObsolete:        return "GateWait";
        case ThreadState::WaitingForProcessInSwap: return "WaitingInSwap";
        case ThreadState::Unknown:
            break;
    }
    return "Unknown";
}

// Maps the raw kernel state value onto the enum, clamping unknown values.
[[nodiscard]] constexpr ThreadState threadStateFromRaw(std::uint32_t raw) noexcept {
    return (raw <= static_cast<std::uint32_t>(ThreadState::WaitingForProcessInSwap))
               ? static_cast<ThreadState>(raw)
               : ThreadState::Unknown;
}

// Always-available fields from the snapshot's inline thread array. No thread
// handle is opened to obtain these.
struct ThreadInfo {
    std::uint32_t tid = 0;
    std::uint32_t ownerPid = 0;
    ThreadState state = ThreadState::Unknown;
    std::uint32_t waitReason = 0;    // KWAIT_REASON (raw)
    std::int32_t priority = 0;
    std::int32_t basePriority = 0;
    std::uint64_t startAddress = 0;  // kernel start routine (see header note)

    std::uint64_t createTime = 0;    // FILETIME (100 ns since 1601-01-01 UTC)
    std::uint64_t kernelTime = 0;    // cumulative, 100 ns units
    std::uint64_t userTime = 0;      // cumulative, 100 ns units
    std::uint32_t contextSwitches = 0;
};

// On-demand fields obtained by opening the thread (may fail per-field).
struct ThreadDetails {
    std::uint64_t win32StartAddress = 0;  // real user entry (NtQueryInformationThread)
    std::uint64_t tebBaseAddress = 0;     // TEB address in the owning process
};

}  // namespace wis::core
