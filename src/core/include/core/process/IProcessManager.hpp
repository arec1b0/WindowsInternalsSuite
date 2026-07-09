#pragma once

// Abstraction over process enumeration and detail retrieval (DIP).
// UI/viewmodels depend on this interface, not the concrete implementation.

#include <cstdint>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/process/ProcessInfo.hpp"

namespace wis::core {

class IProcessManager {
public:
    virtual ~IProcessManager() = default;

    // Cheap system-wide process list from a single native snapshot.
    [[nodiscard]] virtual Result<std::vector<ProcessInfo>, ErrorCode> snapshot() const = 0;

    // Opens the process to retrieve path/command line/architecture. Best-effort:
    // individual fields stay empty/Unknown when access is denied.
    [[nodiscard]] virtual Result<ProcessDetails, ErrorCode> queryDetails(
        std::uint32_t pid) const = 0;
};

}  // namespace wis::core