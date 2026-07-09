#pragma once

// Wrappers over NtQueryInformationThread.

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::thread_info {

// THREAD_BASIC_INFORMATION (exposes the TEB base address and priorities).
[[nodiscard]] Result<ThreadBasicInformation, ErrorCode> queryBasic(HANDLE thread);

// The user-mode entry point the thread was created with (Win32StartAddress).
[[nodiscard]] Result<void*, ErrorCode> queryWin32StartAddress(HANDLE thread);

}  // namespace wis::ntapi::thread_info