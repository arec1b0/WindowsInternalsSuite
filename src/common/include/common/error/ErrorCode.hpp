#pragma once

// ErrorCode: a domain error type unifying Win32 DWORD and NTSTATUS codes.
// Carries an optional human-readable message resolved lazily on demand.

#include <cstdint>
#include <string>

namespace wis {

enum class ErrorDomain : std::uint8_t {
    None,         // no error / uninitialized
    Win32,        // GetLastError()-style DWORD
    NtStatus,     // NTSTATUS from native API
    Application,  // parser/logic error internal to this suite
};

class ErrorCode {
public:
    ErrorCode() noexcept = default;

    // Factories keep call sites explicit about the code domain.
    [[nodiscard]] static ErrorCode fromWin32(std::uint32_t code) noexcept;
    [[nodiscard]] static ErrorCode fromNtStatus(std::int32_t status) noexcept;
    [[nodiscard]] static ErrorCode fromLastError() noexcept;  // captures GetLastError()
    [[nodiscard]] static ErrorCode application(std::string message) noexcept;

    [[nodiscard]] ErrorDomain domain() const noexcept { return domain_; }
    [[nodiscard]] std::uint32_t code() const noexcept { return code_; }
    [[nodiscard]] bool isError() const noexcept { return domain_ != ErrorDomain::None; }

    // Resolves a descriptive message (FormatMessage for Win32/NtStatus).
    [[nodiscard]] std::string message() const;

private:
    ErrorCode(ErrorDomain domain, std::uint32_t code, std::string message) noexcept;

    ErrorDomain domain_ = ErrorDomain::None;
    std::uint32_t code_ = 0;
    std::string context_;  // application message or extra context
};

}  // namespace wis