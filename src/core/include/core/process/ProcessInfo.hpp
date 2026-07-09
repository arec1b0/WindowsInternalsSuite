#pragma once

// Domain model for a process. Split into two tiers:
//   ProcessInfo    - cheap fields available directly from the system snapshot.
//   ProcessDetails - fields requiring the process to be opened (best-effort).

#include <cstdint>
#include <string>
#include <string_view>

namespace wis::core {

enum class Architecture : std::uint8_t {
    Unknown,
    X86,
    X64,
    Arm64,
};

[[nodiscard]] constexpr std::string_view toString(Architecture arch) noexcept {
    switch (arch) {
        case Architecture::X86:   return "x86";
        case Architecture::X64:   return "x64";
        case Architecture::Arm64: return "ARM64";
        case Architecture::Unknown:
            break;
    }
    return "unknown";
}

// Always-available fields sourced from NtQuerySystemInformation. No process
// handle is opened to obtain these, so they are populated even for protected
// processes the caller cannot open.
struct ProcessInfo {
    std::uint32_t pid = 0;
    std::uint32_t parentPid = 0;
    std::wstring imageName;          // executable base name (no path)
    std::uint32_t sessionId = 0;
    std::uint32_t threadCount = 0;
    std::uint32_t handleCount = 0;
    std::int32_t basePriority = 0;

    std::uint64_t createTime = 0;    // FILETIME (100 ns since 1601-01-01 UTC)
    std::uint64_t kernelTime = 0;    // cumulative, 100 ns units
    std::uint64_t userTime = 0;      // cumulative, 100 ns units
    std::uint64_t cycleTime = 0;     // cumulative CPU cycles

    std::uint64_t workingSetBytes = 0;
    std::uint64_t privateBytes = 0;
    std::uint64_t virtualSizeBytes = 0;
};

// On-demand fields obtained by opening the process (may fail per-field).
struct ProcessDetails {
    std::wstring fullImagePath;      // DOS path, e.g. C:\Windows\System32\...
    std::wstring commandLine;
    Architecture architecture = Architecture::Unknown;
};

}  // namespace wis::core