#pragma once

// Domain model for a process access token: identity, integrity, privileges,
// groups, session, and elevation state.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wis::core {

// Windows integrity levels, derived from the token's mandatory-label SID.
enum class IntegrityLevel : std::uint8_t {
    Untrusted,
    Low,
    Medium,
    MediumPlus,
    High,
    System,
    Protected,
    Unknown,
};

[[nodiscard]] constexpr std::string_view toString(IntegrityLevel level) noexcept {
    switch (level) {
        case IntegrityLevel::Untrusted:  return "Untrusted";
        case IntegrityLevel::Low:        return "Low";
        case IntegrityLevel::Medium:     return "Medium";
        case IntegrityLevel::MediumPlus: return "Medium+";
        case IntegrityLevel::High:       return "High";
        case IntegrityLevel::System:     return "System";
        case IntegrityLevel::Protected:  return "Protected";
        case IntegrityLevel::Unknown:
            break;
    }
    return "Unknown";
}

// UAC elevation type reported by TokenElevationType.
enum class ElevationType : std::uint8_t {
    Default,  // token not split (UAC off, or standard user)
    Full,     // elevated (split-token admin running elevated)
    Limited,  // filtered (split-token admin running unelevated)
    Unknown,
};

[[nodiscard]] constexpr std::string_view toString(ElevationType type) noexcept {
    switch (type) {
        case ElevationType::Default: return "Default";
        case ElevationType::Full:    return "Full";
        case ElevationType::Limited: return "Limited";
        case ElevationType::Unknown:
            break;
    }
    return "Unknown";
}

struct TokenPrivilege {
    std::wstring name;              // e.g. "SeDebugPrivilege"
    bool enabled = false;
    bool enabledByDefault = false;
    bool removed = false;
};

struct TokenGroup {
    std::wstring sid;              // string SID (S-1-5-...)
    std::wstring accountName;      // DOMAIN\Name when resolvable
    bool enabled = false;
    bool denyOnly = false;        // SE_GROUP_USE_FOR_DENY_ONLY
    bool integrity = false;       // SE_GROUP_INTEGRITY
    bool logonId = false;         // SE_GROUP_LOGON_ID
};

struct TokenInfo {
    std::wstring userSid;
    std::wstring userName;         // DOMAIN\User when resolvable
    IntegrityLevel integrityLevel = IntegrityLevel::Unknown;
    std::uint32_t sessionId = 0;
    bool isElevated = false;
    ElevationType elevationType = ElevationType::Unknown;
    std::vector<TokenPrivilege> privileges;
    std::vector<TokenGroup> groups;
};

}  // namespace wis::core
