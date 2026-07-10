#include "core/module/FileMetadata.hpp"

#include <windows.h>

#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>

#include <string>
#include <vector>

namespace wis::core {
namespace {

// Retrieves one localized string field from a version resource block. The
// translation code selects the language\codepage sub-block to query.
std::wstring queryVersionString(const std::vector<std::byte>& block,
                                std::uint16_t language, std::uint16_t codePage,
                                const wchar_t* field) {
    wchar_t subBlock[128] = {};
    std::swprintf(subBlock, 128, L"\\StringFileInfo\\%04x%04x\\%s", language, codePage,
                  field);

    void* value = nullptr;
    UINT valueLength = 0;
    if (::VerQueryValueW(block.data(), subBlock, &value, &valueLength) == FALSE ||
        value == nullptr || valueLength == 0) {
        return {};
    }
    // valueLength counts characters including the terminator.
    return std::wstring(static_cast<const wchar_t*>(value), valueLength - 1);
}

// The translation table pairs a language id with a codepage; the first entry
// is used to address the StringFileInfo sub-blocks.
struct Translation {
    std::uint16_t language;
    std::uint16_t codePage;
};

bool firstTranslation(const std::vector<std::byte>& block, Translation& out) {
    void* data = nullptr;
    UINT length = 0;
    if (::VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation", &data, &length) ==
            FALSE ||
        data == nullptr || length < sizeof(Translation)) {
        return false;
    }
    // Each entry is two WORDs: [language][codepage].
    const auto* raw = static_cast<const std::uint16_t*>(data);
    out.language = raw[0];
    out.codePage = raw[1];
    return true;
}

}  // namespace

VersionMetadata queryVersionMetadata(std::wstring_view filePath) {
    VersionMetadata meta;
    const std::wstring path(filePath);

    DWORD handle = 0;
    const DWORD size = ::GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return meta;  // no version resource
    }

    std::vector<std::byte> block(size);
    if (::GetFileVersionInfoW(path.c_str(), handle, size, block.data()) == FALSE) {
        return meta;
    }

    Translation translation{0x0409, 0x04B0};  // default: US English, Unicode
    firstTranslation(block, translation);

    meta.companyName =
        queryVersionString(block, translation.language, translation.codePage, L"CompanyName");
    meta.productName =
        queryVersionString(block, translation.language, translation.codePage, L"ProductName");
    meta.fileDescription = queryVersionString(block, translation.language,
                                              translation.codePage, L"FileDescription");
    meta.fileVersion =
        queryVersionString(block, translation.language, translation.codePage, L"FileVersion");
    meta.productVersion = queryVersionString(block, translation.language,
                                             translation.codePage, L"ProductVersion");
    meta.originalFilename = queryVersionString(block, translation.language,
                                               translation.codePage, L"OriginalFilename");

    meta.present = !meta.companyName.empty() || !meta.productName.empty() ||
                   !meta.fileVersion.empty() || !meta.fileDescription.empty();
    return meta;
}

SignatureInfo querySignature(std::wstring_view filePath) {
    SignatureInfo info;
    const std::wstring path(filePath);

    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();

    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;  // avoid network CRL latency
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID actionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = ::WinVerifyTrust(nullptr, &actionId, &trustData);

    switch (status) {
        case ERROR_SUCCESS:
            info.status = SignatureStatus::Signed;
            info.detail = L"Authenticode signature verified";
            break;
        case TRUST_E_NOSIGNATURE:
            info.status = SignatureStatus::Unsigned;
            info.detail = L"No embedded signature";
            break;
        case TRUST_E_EXPLICIT_DISTRUST:
        case TRUST_E_SUBJECT_NOT_TRUSTED:
        case CERT_E_UNTRUSTEDROOT:
            info.status = SignatureStatus::Untrusted;
            info.detail = L"Signature present but not trusted";
            break;
        default:
            info.status = SignatureStatus::Error;
            info.detail = L"Verification did not complete";
            break;
    }

    // Release the WinVerifyTrust state allocated by the VERIFY action.
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    ::WinVerifyTrust(nullptr, &actionId, &trustData);

    return info;
}

}  // namespace wis::core
