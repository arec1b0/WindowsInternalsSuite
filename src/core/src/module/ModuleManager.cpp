#include "core/module/ModuleManager.hpp"

#include <windows.h>

#include <utility>

#include "core/module/FileMetadata.hpp"

namespace wis::core {
namespace {

// Query + read module list of a foreign process. PROCESS_VM_READ is required by
// EnumProcessModulesEx to walk the loader data.
constexpr ACCESS_MASK kModuleAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

// Extracts the base file name from a full path (last '\\' or '/').
std::wstring baseName(const std::wstring& fullPath) {
    const std::size_t slash = fullPath.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? fullPath : fullPath.substr(slash + 1);
}

}  // namespace

ModuleManager::ModuleManager(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<ModuleInfo>, ErrorCode> ModuleManager::snapshot(std::uint32_t pid) const {
    auto handleResult = native_.openProcess(pid, kModuleAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle process = std::move(handleResult).value();

    auto basesResult = native_.enumProcessModules(process.get());
    if (!basesResult) {
        return makeUnexpected(std::move(basesResult).error());
    }

    std::vector<ModuleInfo> modules;
    modules.reserve(basesResult.value().size());

    for (const std::uint64_t base : basesResult.value()) {
        ModuleInfo module;
        module.baseAddress = base;

        auto pathResult = native_.queryModuleFileName(process.get(), base);
        if (pathResult) {
            module.fullPath = std::move(pathResult).value();
            module.name = baseName(module.fullPath);
        }

        auto rawResult = native_.queryModuleInformation(process.get(), base);
        if (rawResult) {
            module.entryPoint = rawResult.value().entryPoint;
            module.sizeOfImage = rawResult.value().sizeOfImage;
        }

        // Cheap version fields read from the file resource; failure is tolerated.
        if (!module.fullPath.empty()) {
            const VersionMetadata meta = queryVersionMetadata(module.fullPath);
            module.companyName = meta.companyName;
            module.fileVersion = meta.fileVersion;
        }

        modules.push_back(std::move(module));
    }

    return modules;
}

SignatureInfo ModuleManager::querySignature(std::wstring_view filePath) const {
    return core::querySignature(filePath);
}

}  // namespace wis::core
