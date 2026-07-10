#pragma once

// Best-effort file metadata helpers used by the Module Explorer:
//   - version information (company, product, file/product version strings)
//   - Authenticode signature verification (embedded and, optionally, catalog)
// Both operate on an on-disk file path and degrade gracefully when the data is
// absent (an unsigned or version-less file is a valid, non-error outcome).

#include <cstdint>
#include <string>
#include <string_view>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"

namespace wis::core {

struct VersionMetadata {
    bool present = false;
    std::wstring companyName;
    std::wstring productName;
    std::wstring fileDescription;
    std::wstring fileVersion;
    std::wstring productVersion;
    std::wstring originalFilename;
};

enum class SignatureStatus : std::uint8_t {
    Unchecked,
    Signed,        // trust verified
    Unsigned,      // no signature present
    Untrusted,     // signature present but failed verification
    Error,         // verification could not complete
};

[[nodiscard]] constexpr std::string_view toString(SignatureStatus status) noexcept {
    switch (status) {
        case SignatureStatus::Unchecked: return "Unchecked";
        case SignatureStatus::Signed:    return "Signed";
        case SignatureStatus::Unsigned:  return "Unsigned";
        case SignatureStatus::Untrusted: return "Untrusted";
        case SignatureStatus::Error:     return "Error";
    }
    return "Unknown";
}

struct SignatureInfo {
    SignatureStatus status = SignatureStatus::Unchecked;
    std::wstring signerName;   // subject common name, when resolvable
    std::wstring detail;       // human-readable status detail
};

// Reads version resource fields. Returns a default (present == false) when the
// file has no version resource.
[[nodiscard]] VersionMetadata queryVersionMetadata(std::wstring_view filePath);

// Verifies the file's Authenticode signature via WinVerifyTrust. This touches
// the disk and may be slow; call it on demand, not while listing modules.
[[nodiscard]] SignatureInfo querySignature(std::wstring_view filePath);

}  // namespace wis::core
