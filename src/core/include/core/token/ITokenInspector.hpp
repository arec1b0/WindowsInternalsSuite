#pragma once

// Abstraction over token inspection (DIP), so TokenExplorerVM can be unit-tested
// against a fake without a live process token.

#include <cstdint>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/token/TokenInfo.hpp"

namespace wis::core {

class ITokenInspector {
public:
    virtual ~ITokenInspector() = default;

    // Reads the access token of the process identified by `pid`. Individual
    // fields degrade gracefully (empty/Unknown) when a sub-query is denied;
    // the call fails only when the token cannot be opened at all.
    [[nodiscard]] virtual Result<TokenInfo, ErrorCode> inspect(std::uint32_t pid) const = 0;
};

}  // namespace wis::core
