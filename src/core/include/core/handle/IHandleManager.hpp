#pragma once

// Abstraction over system handle enumeration and on-demand name resolution (DIP).

#include <cstdint>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/handle/HandleInfo.hpp"

namespace wis::core {

class IHandleManager {
public:
    virtual ~IHandleManager() = default;

    // All open handles owned by `pid`, with type/access/address resolved from a
    // single system snapshot. Object names are NOT resolved here (see below).
    [[nodiscard]] virtual Result<std::vector<HandleInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const = 0;

    // Resolves the name of one handle by duplicating it and querying the object.
    // Guarded by a timeout because the query can block on some object types.
    [[nodiscard]] virtual HandleObjectName resolveName(std::uint32_t ownerPid,
                                                       std::uint64_t handleValue) const = 0;
};

}  // namespace wis::core
