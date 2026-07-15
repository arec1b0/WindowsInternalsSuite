#pragma once

// Abstraction over system-wide information gathering (DIP).

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/system/SystemInfo.hpp"

namespace wis::core {

class ISystemInfoProvider {
public:
    virtual ~ISystemInfoProvider() = default;

    // Collects a full system snapshot. Individual sections degrade gracefully
    // (empty/zero) when a sub-query fails; the call fails only when nothing
    // could be gathered.
    [[nodiscard]] virtual Result<SystemInfo, ErrorCode> collect() const = 0;
};

}  // namespace wis::core
