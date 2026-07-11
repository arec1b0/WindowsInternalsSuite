#pragma once

// Abstraction over heap inspection (DIP).

#include <cstdint>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/heap/HeapInfo.hpp"

namespace wis::core {

class IHeapInspector {
public:
    virtual ~IHeapInspector() = default;

    // Per-heap summaries for `pid`. Does not enumerate individual blocks (that
    // would freeze the target's heaps); block counts come from summary metadata.
    [[nodiscard]] virtual Result<std::vector<HeapInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const = 0;
};

}  // namespace wis::core
