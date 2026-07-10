#pragma once

// Abstraction over module enumeration and on-demand signature checks (DIP).

#include <cstdint>
#include <string_view>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/module/FileMetadata.hpp"
#include "core/module/ModuleInfo.hpp"

namespace wis::core {

class IModuleManager {
public:
    virtual ~IModuleManager() = default;

    // Loaded modules for `pid`, with base/entry/size and cheap version fields.
    // Signature status is left Unchecked; resolve it per-module via querySignature.
    [[nodiscard]] virtual Result<std::vector<ModuleInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const = 0;

    // Verifies a module image's Authenticode signature (slow; on demand).
    [[nodiscard]] virtual SignatureInfo querySignature(std::wstring_view filePath) const = 0;
};

}  // namespace wis::core
