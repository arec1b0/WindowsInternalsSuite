#pragma once

// Concrete ITokenInspector backed by the native API seam (for opening the
// process) plus advapi32 token queries.

#include <cstdint>

#include "core/token/ITokenInspector.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class TokenInspector final : public ITokenInspector {
public:
    // `native` must outlive this inspector (owned by the composition root).
    explicit TokenInspector(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<TokenInfo, ErrorCode> inspect(std::uint32_t pid) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
