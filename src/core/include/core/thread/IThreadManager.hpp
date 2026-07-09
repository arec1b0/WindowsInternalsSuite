#pragma once

// Abstraction over thread enumeration and detail retrieval (DIP).

#include <cstdint>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/thread/ThreadInfo.hpp"

namespace wis::core {

class IThreadManager {
public:
    virtual ~IThreadManager() = default;

    // Threads belonging to `pid`, extracted from a single process snapshot.
    // An empty vector is a valid result (e.g. the process just exited).
    [[nodiscard]] virtual Result<std::vector<ThreadInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const = 0;

    // Opens the thread to retrieve its Win32 start address and TEB base.
    [[nodiscard]] virtual Result<ThreadDetails, ErrorCode> queryDetails(
        std::uint32_t tid) const = 0;
};

}  // namespace wis::core
