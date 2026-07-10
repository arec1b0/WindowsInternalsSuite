#pragma once

// Concrete IModuleManager backed by the native API seam plus file-metadata
// helpers. Enumeration opens the process with minimal access and closes it via
// RAII; signature verification is deferred to querySignature.

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/module/IModuleManager.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class ModuleManager final : public IModuleManager {
public:
    // `native` must outlive this manager (owned by the composition root).
    explicit ModuleManager(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<ModuleInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const override;
    [[nodiscard]] SignatureInfo querySignature(std::wstring_view filePath) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
