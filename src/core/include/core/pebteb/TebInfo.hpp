#pragma once

// Domain model for a thread's TEB, as read from the owning process.

#include <cstdint>
#include <vector>

namespace wis::core {

struct TebInfo {
    std::uint64_t address = 0;               // VA of the TEB
    std::uint64_t stackBase = 0;             // NtTib.StackBase (high address)
    std::uint64_t stackLimit = 0;            // NtTib.StackLimit (low address)
    std::uint64_t selfPointer = 0;           // NtTib.Self (should equal address)
    std::uint64_t pebAddress = 0;            // ProcessEnvironmentBlock
    std::uint64_t tlsPointer = 0;            // ThreadLocalStoragePointer
    std::uint32_t lastErrorValue = 0;
    std::uint32_t ownerPid = 0;
    std::uint32_t tid = 0;

    // TLS slot values (64 entries on x64). Empty when the slots could not be read.
    std::vector<std::uint64_t> tlsSlots;

    // Stack region size derived from base/limit (committed stack span).
    [[nodiscard]] std::uint64_t stackSize() const noexcept {
        return (stackBase > stackLimit) ? (stackBase - stackLimit) : 0;
    }
};

}  // namespace wis::core
