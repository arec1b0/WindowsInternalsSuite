#pragma once

// Free helpers translating raw OS status codes into readable strings.
// Kept separate from ErrorCode so message formatting can be unit-tested
// and reused without constructing an ErrorCode.

#include <cstdint>
#include <string>

namespace wis::system_error {

// Formats a Win32 error (from GetLastError) via FormatMessage.
[[nodiscard]] std::string formatWin32Message(std::uint32_t code);

// Formats an NTSTATUS via FormatMessage against ntdll's message table.
[[nodiscard]] std::string formatNtStatusMessage(std::int32_t status);

}  // namespace wis::system_error