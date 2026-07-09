#pragma once

// Wrappers over NtOpenProcess and NtQueryInformationProcess.

#include <cstdint>
#include <string>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::process_info {

// Opens a process by PID with the requested access mask.
[[nodiscard]] Result<Handle, ErrorCode> open(std::uint32_t pid, ACCESS_MASK desiredAccess);

// PROCESS_BASIC_INFORMATION (exposes the PEB base address).
[[nodiscard]] Result<ProcessBasicInformation, ErrorCode> queryBasic(HANDLE process);

// Full command line (Win8.1+). Empty string for processes without one.
[[nodiscard]] Result<std::wstring, ErrorCode> queryCommandLine(HANDLE process);

// Native device-path image name (e.g. \Device\HarddiskVolume3\...\app.exe).
[[nodiscard]] Result<std::wstring, ErrorCode> queryImageName(HANDLE process);

}  // namespace wis::ntapi::process_info