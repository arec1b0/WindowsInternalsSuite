#pragma once

// Domain model for a loaded module (DLL/EXE) within a process.

#include <cstdint>
#include <string>

#include "core/module/FileMetadata.hpp"

namespace wis::core {

struct ModuleInfo {
    std::wstring name;              // base file name (e.g. "kernel32.dll")
    std::wstring fullPath;          // fully-qualified image path
    std::uint64_t baseAddress = 0;
    std::uint64_t entryPoint = 0;
    std::uint32_t sizeOfImage = 0;

    // Version metadata (populated during listing; cheap file-resource read).
    std::wstring companyName;
    std::wstring fileVersion;

    // Signature is left Unchecked during listing and resolved on demand.
    SignatureStatus signatureStatus = SignatureStatus::Unchecked;
};

}  // namespace wis::core
