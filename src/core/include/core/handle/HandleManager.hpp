#pragma once

// Concrete IHandleManager backed by the native API seam. Enumeration is a single
// system snapshot filtered by PID and annotated with a prebuilt type table.
// Name resolution duplicates the target handle into this process and queries it
// under a timeout, because NtQueryObject can block on certain object types.

#include <cstdint>
#include <vector>

#include "core/handle/IHandleManager.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class HandleManager final : public IHandleManager {
public:
    // `native` must outlive this manager (owned by the composition root).
    explicit HandleManager(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<HandleInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const override;
    [[nodiscard]] HandleObjectName resolveName(
        std::uint32_t ownerPid, std::uint64_t handleValue) const override;

    // Milliseconds allowed for a single object-name query before giving up.
    static constexpr std::uint32_t kNameQueryTimeoutMs = 100;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
