#pragma once

// NTSTATUS type and helpers. NTSTATUS is a 32-bit signed value where the top
// two bits encode severity; negative values are errors.

#include <windows.h>

namespace wis::ntapi {

using NtStatus = LONG;  // matches the SDK NTSTATUS typedef

// Success covers both STATUS_SUCCESS and informational/warning-but-usable codes
// per the NT_SUCCESS convention (status >= 0).
[[nodiscard]] constexpr bool ntSuccess(NtStatus status) noexcept {
    return status >= 0;
}

[[nodiscard]] constexpr bool ntError(NtStatus status) noexcept {
    return (static_cast<unsigned long>(status) >> 30) == 3U;
}

// Frequently-checked status codes for buffer-sizing loops and access failures.
namespace status {
constexpr NtStatus Success             = static_cast<NtStatus>(0x00000000UL);
constexpr NtStatus BufferOverflow      = static_cast<NtStatus>(0x80000005UL);
constexpr NtStatus InfoLengthMismatch  = static_cast<NtStatus>(0xC0000004UL);
constexpr NtStatus BufferTooSmall      = static_cast<NtStatus>(0xC0000023UL);
constexpr NtStatus AccessDenied        = static_cast<NtStatus>(0xC0000022UL);
constexpr NtStatus InvalidHandle       = static_cast<NtStatus>(0xC0000008UL);
constexpr NtStatus InvalidParameter    = static_cast<NtStatus>(0xC000000DUL);
constexpr NtStatus NotImplemented      = static_cast<NtStatus>(0xC0000002UL);
constexpr NtStatus PartialCopy         = static_cast<NtStatus>(0x8000000DUL);
}  // namespace status

}  // namespace wis::ntapi