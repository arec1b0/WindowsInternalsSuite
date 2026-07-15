#pragma once

// Wrappers over NtQuerySystemInformation. Enumeration queries return owned raw
// buffers; the caller walks the variable-length record chains (NextEntryOffset).

#include <cstddef>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::system_info {

// Snapshot of all processes (SystemProcessInformation chain + inline threads).
[[nodiscard]] Result<std::vector<std::byte>, ErrorCode> queryProcesses();

// Snapshot of all open handles system-wide (SystemHandleInformationEx).
[[nodiscard]] Result<std::vector<std::byte>, ErrorCode> queryHandles();

// Fixed-size basic system info (page size, processor count, address bounds).
[[nodiscard]] Result<SystemBasicInformation, ErrorCode> queryBasic();

// Pagefile usage records (SystemPagefileInformation chain). An empty result
// means the system has no pagefile configured.
[[nodiscard]] Result<std::vector<std::byte>, ErrorCode> queryPagefiles();

// True OS version via RtlGetVersion (not subject to manifest shimming).
[[nodiscard]] Result<RTL_OSVERSIONINFOEXW, ErrorCode> queryOsVersion();

}  // namespace wis::ntapi::system_info