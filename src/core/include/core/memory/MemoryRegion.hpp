#pragma once

// Domain model for one virtual-memory region, mapped from
// MEMORY_BASIC_INFORMATION. State/Type are normalized enums; protection is kept
// as the raw flag word with an inline formatter (a region's protection is a
// bitfield, not a single value).

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace wis::core {

enum class MemoryState : std::uint8_t {
    Free,      // MEM_FREE     - address range not backed by anything
    Reserved,  // MEM_RESERVE  - reserved but not committed
    Committed, // MEM_COMMIT   - committed, accessible per protection
    Unknown,
};

enum class MemoryType : std::uint8_t {
    None,     // free regions have no type
    Image,    // MEM_IMAGE    - backed by a mapped executable/DLL
    Mapped,   // MEM_MAPPED   - backed by a mapped data file/section
    Private,  // MEM_PRIVATE  - private committed memory (heap, stack, ...)
    Unknown,
};

[[nodiscard]] constexpr std::string_view toString(MemoryState state) noexcept {
    switch (state) {
        case MemoryState::Free:      return "MEM_FREE";
        case MemoryState::Reserved:  return "MEM_RESERVE";
        case MemoryState::Committed: return "MEM_COMMIT";
        case MemoryState::Unknown:
            break;
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view toString(MemoryType type) noexcept {
    switch (type) {
        case MemoryType::None:    return "-";
        case MemoryType::Image:   return "IMAGE";
        case MemoryType::Mapped:  return "MAPPED";
        case MemoryType::Private: return "PRIVATE";
        case MemoryType::Unknown:
            break;
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr MemoryState memoryStateFromRaw(DWORD state) noexcept {
    switch (state) {
        case MEM_FREE:    return MemoryState::Free;
        case MEM_RESERVE: return MemoryState::Reserved;
        case MEM_COMMIT:  return MemoryState::Committed;
        default:          return MemoryState::Unknown;
    }
}

[[nodiscard]] constexpr MemoryType memoryTypeFromRaw(DWORD type) noexcept {
    switch (type) {
        case 0:           return MemoryType::None;
        case MEM_IMAGE:   return MemoryType::Image;
        case MEM_MAPPED:  return MemoryType::Mapped;
        case MEM_PRIVATE: return MemoryType::Private;
        default:          return MemoryType::Unknown;
    }
}

// Compact protection string, e.g. "RW", "RX", "RWX", "NA", plus modifier
// suffixes (+G guard, +NC nocache, +WC writecombine). Free regions -> "-".
[[nodiscard]] inline std::string protectionToString(std::uint32_t protect) {
    if (protect == 0) {
        return "-";
    }

    // Access bits occupy the low byte; modifiers are the higher bits.
    const std::uint32_t base = protect & 0xFFU;

    std::string result;
    switch (base) {
        case PAGE_NOACCESS:          result = "NA";  break;
        case PAGE_READONLY:          result = "R";   break;
        case PAGE_READWRITE:         result = "RW";  break;
        case PAGE_WRITECOPY:         result = "WC";  break;
        case PAGE_EXECUTE:           result = "X";   break;
        case PAGE_EXECUTE_READ:      result = "RX";  break;
        case PAGE_EXECUTE_READWRITE: result = "RWX"; break;
        case PAGE_EXECUTE_WRITECOPY: result = "WCX"; break;
        default:                     result = "?";   break;
    }

    if ((protect & PAGE_GUARD) != 0U) {
        result += " +G";
    }
    if ((protect & PAGE_NOCACHE) != 0U) {
        result += " +NC";
    }
    if ((protect & PAGE_WRITECOMBINE) != 0U) {
        result += " +WC";
    }
    return result;
}

// True when the region's protection permits reading via ReadProcessMemory /
// NtReadVirtualMemory. Guard pages and no-access pages are excluded: touching a
// guard page raises an exception in the target, and no-access always fails.
[[nodiscard]] inline bool isReadable(std::uint32_t protect) noexcept {
    if (protect == 0) {
        return false;
    }
    if ((protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return false;
    }
    constexpr std::uint32_t readableMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                           PAGE_EXECUTE_WRITECOPY;
    return (protect & readableMask) != 0U;
}

struct MemoryRegion {
    std::uint64_t baseAddress = 0;
    std::uint64_t allocationBase = 0;
    std::uint64_t regionSize = 0;
    MemoryState state = MemoryState::Unknown;
    MemoryType type = MemoryType::None;
    std::uint32_t protect = 0;            // raw Protect flags
    std::uint32_t allocationProtect = 0;  // raw AllocationProtect flags
};

}  // namespace wis::core
