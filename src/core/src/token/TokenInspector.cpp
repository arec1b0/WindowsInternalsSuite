#include "core/token/TokenInspector.hpp"

#include <windows.h>

#include <sddl.h>  // ConvertSidToStringSidW

#include <cstddef>
#include <utility>
#include <vector>

#include "common/raii/UniqueHandle.hpp"
#include "common/raii/UniqueLocalMem.hpp"

namespace wis::core {
namespace {

// Access needed to open the process and then its token for querying.
constexpr ACCESS_MASK kProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION;

// Growing-buffer wrapper around GetTokenInformation for variable-length classes.
Result<std::vector<std::byte>, ErrorCode> queryTokenInformation(
    HANDLE token, TOKEN_INFORMATION_CLASS infoClass) {
    DWORD needed = 0;
    ::GetTokenInformation(token, infoClass, nullptr, 0, &needed);
    if (needed == 0) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    std::vector<std::byte> buffer(needed);
    DWORD used = needed;
    if (::GetTokenInformation(token, infoClass, buffer.data(), needed, &used) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }
    buffer.resize(used);
    return buffer;
}

// Fixed-size token value query (SessionId, Elevation, ElevationType).
template <typename T>
[[nodiscard]] bool queryTokenValue(HANDLE token, TOKEN_INFORMATION_CLASS infoClass, T& out) {
    DWORD used = 0;
    return ::GetTokenInformation(token, infoClass, &out, sizeof(out), &used) != FALSE;
}

// Converts a SID to its canonical string form (S-1-5-...).
std::wstring sidToString(PSID sid) {
    if (sid == nullptr || ::IsValidSid(sid) == FALSE) {
        return {};
    }
    LPWSTR raw = nullptr;
    if (::ConvertSidToStringSidW(sid, &raw) == FALSE) {
        return {};
    }
    UniqueLocalMem<LPWSTR> owned(raw);  // LocalFree on scope exit
    return std::wstring(raw);
}

// Resolves a SID to DOMAIN\Name, or an empty string when it cannot be resolved.
std::wstring sidToAccountName(PSID sid) {
    if (sid == nullptr || ::IsValidSid(sid) == FALSE) {
        return {};
    }

    DWORD nameLength = 0;
    DWORD domainLength = 0;
    SID_NAME_USE use = SidTypeUnknown;
    ::LookupAccountSidW(nullptr, sid, nullptr, &nameLength, nullptr, &domainLength, &use);
    if (nameLength == 0 && domainLength == 0) {
        return {};
    }

    std::wstring name(nameLength, L'\0');
    std::wstring domain(domainLength, L'\0');
    if (::LookupAccountSidW(nullptr, sid, name.data(), &nameLength, domain.data(),
                            &domainLength, &use) == FALSE) {
        return {};
    }
    name.resize(nameLength);
    domain.resize(domainLength);

    if (domain.empty()) {
        return name;
    }
    return domain + L"\\" + name;
}

// Resolves a privilege LUID to its programmatic name (e.g. "SeDebugPrivilege").
std::wstring privilegeName(const LUID& luid) {
    LUID local = luid;
    DWORD length = 0;
    ::LookupPrivilegeNameW(nullptr, &local, nullptr, &length);
    if (length == 0) {
        return {};
    }
    std::wstring name(length, L'\0');  // length includes the terminator here
    if (::LookupPrivilegeNameW(nullptr, &local, name.data(), &length) == FALSE) {
        return {};
    }
    name.resize(length);  // length now excludes the terminator
    return name;
}

// Maps a mandatory-label integrity RID to the domain enum.
IntegrityLevel integrityFromRid(DWORD rid) noexcept {
    if (rid >= SECURITY_MANDATORY_PROTECTED_PROCESS_RID) {
        return IntegrityLevel::Protected;
    }
    if (rid >= SECURITY_MANDATORY_SYSTEM_RID) {
        return IntegrityLevel::System;
    }
    if (rid >= SECURITY_MANDATORY_HIGH_RID) {
        return IntegrityLevel::High;
    }
    if (rid > SECURITY_MANDATORY_MEDIUM_RID) {
        return IntegrityLevel::MediumPlus;
    }
    if (rid >= SECURITY_MANDATORY_MEDIUM_RID) {
        return IntegrityLevel::Medium;
    }
    if (rid >= SECURITY_MANDATORY_LOW_RID) {
        return IntegrityLevel::Low;
    }
    return IntegrityLevel::Untrusted;
}

void fillUser(HANDLE token, TokenInfo& info) {
    auto buffer = queryTokenInformation(token, TokenUser);
    if (!buffer) {
        return;
    }
    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.value().data());
    info.userSid = sidToString(user->User.Sid);
    info.userName = sidToAccountName(user->User.Sid);
}

void fillIntegrity(HANDLE token, TokenInfo& info) {
    auto buffer = queryTokenInformation(token, TokenIntegrityLevel);
    if (!buffer) {
        return;
    }
    const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.value().data());
    PSID sid = label->Label.Sid;
    if (sid == nullptr || ::IsValidSid(sid) == FALSE) {
        return;
    }
    const UCHAR* countPtr = ::GetSidSubAuthorityCount(sid);
    if (countPtr == nullptr || *countPtr == 0) {
        return;
    }
    const DWORD rid = *::GetSidSubAuthority(sid, static_cast<DWORD>(*countPtr - 1));
    info.integrityLevel = integrityFromRid(rid);
}

void fillPrivileges(HANDLE token, TokenInfo& info) {
    auto buffer = queryTokenInformation(token, TokenPrivileges);
    if (!buffer) {
        return;
    }
    const auto* privileges = reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.value().data());
    info.privileges.reserve(privileges->PrivilegeCount);

    for (DWORD i = 0; i < privileges->PrivilegeCount; ++i) {
        const LUID_AND_ATTRIBUTES& entry = privileges->Privileges[i];
        TokenPrivilege privilege;
        privilege.name = privilegeName(entry.Luid);
        privilege.enabled = (entry.Attributes & SE_PRIVILEGE_ENABLED) != 0;
        privilege.enabledByDefault = (entry.Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) != 0;
        privilege.removed = (entry.Attributes & SE_PRIVILEGE_REMOVED) != 0;
        info.privileges.push_back(std::move(privilege));
    }
}

void fillGroups(HANDLE token, TokenInfo& info) {
    auto buffer = queryTokenInformation(token, TokenGroups);
    if (!buffer) {
        return;
    }
    const auto* groups = reinterpret_cast<const TOKEN_GROUPS*>(buffer.value().data());
    info.groups.reserve(groups->GroupCount);

    for (DWORD i = 0; i < groups->GroupCount; ++i) {
        const SID_AND_ATTRIBUTES& entry = groups->Groups[i];
        TokenGroup group;
        group.sid = sidToString(entry.Sid);
        group.accountName = sidToAccountName(entry.Sid);
        group.enabled = (entry.Attributes & SE_GROUP_ENABLED) != 0;
        group.denyOnly = (entry.Attributes & SE_GROUP_USE_FOR_DENY_ONLY) != 0;
        group.integrity = (entry.Attributes & SE_GROUP_INTEGRITY) != 0;
        group.logonId = (entry.Attributes & SE_GROUP_LOGON_ID) != 0;
        info.groups.push_back(std::move(group));
    }
}

void fillSessionAndElevation(HANDLE token, TokenInfo& info) {
    DWORD sessionId = 0;
    if (queryTokenValue(token, TokenSessionId, sessionId)) {
        info.sessionId = sessionId;
    }

    TOKEN_ELEVATION elevation{};
    if (queryTokenValue(token, TokenElevation, elevation)) {
        info.isElevated = elevation.TokenIsElevated != 0;
    }

    TOKEN_ELEVATION_TYPE elevationType = TokenElevationTypeDefault;
    if (queryTokenValue(token, TokenElevationType, elevationType)) {
        switch (elevationType) {
            case TokenElevationTypeDefault: info.elevationType = ElevationType::Default; break;
            case TokenElevationTypeFull:    info.elevationType = ElevationType::Full;    break;
            case TokenElevationTypeLimited: info.elevationType = ElevationType::Limited; break;
            default:                        info.elevationType = ElevationType::Unknown; break;
        }
    }
}

}  // namespace

TokenInspector::TokenInspector(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<TokenInfo, ErrorCode> TokenInspector::inspect(std::uint32_t pid) const {
    auto processResult = native_.openProcess(pid, kProcessAccess);
    if (!processResult) {
        return makeUnexpected(std::move(processResult).error());
    }
    const Handle process = std::move(processResult).value();

    HANDLE rawToken = nullptr;
    if (::OpenProcessToken(process.get(), TOKEN_QUERY, &rawToken) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }
    const Handle token(rawToken);

    // Each sub-query is best-effort; the token is open, so we return whatever we
    // can read rather than failing wholesale on one denied class.
    TokenInfo info;
    fillUser(token.get(), info);
    fillIntegrity(token.get(), info);
    fillPrivileges(token.get(), info);
    fillGroups(token.get(), info);
    fillSessionAndElevation(token.get(), info);

    return info;
}

}  // namespace wis::core
