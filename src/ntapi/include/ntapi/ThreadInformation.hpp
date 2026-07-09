#pragma once

// Wrappers over NtOpenThread and NtQueryInformationThread.

#include <cstdint>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::thread_info {

// Opens a thread by TID with the requested access mask.
[[nodiscard]] Result<Handle, ErrorCode> open(std::uint32_t tid, ACCESS_MASK desiredAccess);

// THREAD_BASIC_INFORMATION (exposes the TEB base address and priorities).
[[nodiscard]] Result<ThreadBasicInformation, ErrorCode> queryBasic(HANDLE thread);

// The user-mode entry point the thread was created with (Win32StartAddress).
[[nodiscard]] Result<void*, ErrorCode> queryWin32StartAddress(HANDLE thread);

}  // namespace wis::ntapi::thread_info