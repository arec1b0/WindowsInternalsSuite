#pragma once

// Domain model for one process heap summary.

#include <cstdint>
#include <string>

namespace wis::core {

struct HeapInfo {
    std::uint64_t baseAddress = 0;
    std::uint32_t flags = 0;
    std::uint64_t bytesAllocated = 0;   // total allocated bytes
    std::uint64_t bytesCommitted = 0;   // committed backing bytes
    std::uint32_t blockCount = 0;       // allocated block count (NumberOfEntries)
    std::uint32_t tagCount = 0;
    bool isDefaultHeap = false;         // matches the process's default heap
};

// Human-readable heap flag string (HEAP_GROWABLE, HEAP_NO_SERIALIZE, ...).
[[nodiscard]] std::string heapFlagsToString(std::uint32_t flags);

}  // namespace wis::core
