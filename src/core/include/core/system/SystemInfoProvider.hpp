#pragma once

// Concrete ISystemInfoProvider. Combines the native seam (OS version, pagefiles,
// basic system information) with Win32 topology/memory queries and CPUID.

#include "core/system/ISystemInfoProvider.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class SystemInfoProvider final : public ISystemInfoProvider {
public:
    // `native` must outlive this provider (owned by the composition root).
    explicit SystemInfoProvider(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<SystemInfo, ErrorCode> collect() const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
