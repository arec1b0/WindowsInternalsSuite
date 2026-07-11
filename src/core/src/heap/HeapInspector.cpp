#include "core/heap/HeapInspector.hpp"

#include <windows.h>

#include <string>
#include <utility>

namespace wis::core {
namespace {

// The first heap returned is conventionally the process default heap.
constexpr std::size_t kDefaultHeapIndex = 0;

}  // namespace

std::string heapFlagsToString(std::uint32_t flags) {
    if (flags == 0) {
        return "-";
    }
    std::string result;
    const auto append = [&](std::uint32_t flag, std::string_view label) {
        if ((flags & flag) != 0) {
            if (!result.empty()) {
                result += " | ";
            }
            result += label;
        }
    };
    append(HEAP_NO_SERIALIZE, "NO_SERIALIZE");
    append(HEAP_GROWABLE, "GROWABLE");
    append(HEAP_GENERATE_EXCEPTIONS, "GEN_EXCEPTIONS");
    append(HEAP_ZERO_MEMORY, "ZERO_MEMORY");
    append(HEAP_REALLOC_IN_PLACE_ONLY, "REALLOC_IN_PLACE");
    append(HEAP_TAIL_CHECKING_ENABLED, "TAIL_CHECK");
    append(HEAP_FREE_CHECKING_ENABLED, "FREE_CHECK");
    append(HEAP_CREATE_ALIGN_16, "ALIGN_16");
    append(HEAP_CREATE_ENABLE_TRACING, "TRACING");
    append(HEAP_CREATE_ENABLE_EXECUTE, "EXECUTABLE");
    return result.empty() ? "-" : result;
}

HeapInspector::HeapInspector(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<HeapInfo>, ErrorCode> HeapInspector::snapshot(std::uint32_t pid) const {
    auto heapsResult = native_.queryProcessHeaps(pid);
    if (!heapsResult) {
        return makeUnexpected(std::move(heapsResult).error());
    }

    const std::vector<ntapi::HeapSummary>& summaries = heapsResult.value();
    std::vector<HeapInfo> heaps;
    heaps.reserve(summaries.size());

    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const ntapi::HeapSummary& summary = summaries[i];
        HeapInfo info;
        info.baseAddress = summary.baseAddress;
        info.flags = summary.flags;
        info.bytesAllocated = summary.bytesAllocated;
        info.bytesCommitted = summary.bytesCommitted;
        info.blockCount = summary.numberOfEntries;
        info.tagCount = summary.numberOfTags;
        info.isDefaultHeap = (i == kDefaultHeapIndex);
        heaps.push_back(info);
    }

    return heaps;
}

}  // namespace wis::core
