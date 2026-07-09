#include "common/error/ErrorCode.hpp"

#include <windows.h>

#include <utility>

#include "common/error/SystemError.hpp"

namespace wis {

ErrorCode::ErrorCode(ErrorDomain domain, std::uint32_t code, std::string message) noexcept
    : domain_(domain), code_(code), context_(std::move(message)) {}

ErrorCode ErrorCode::fromWin32(std::uint32_t code) noexcept {
    return ErrorCode(ErrorDomain::Win32, code, {});
}

ErrorCode ErrorCode::fromNtStatus(std::int32_t status) noexcept {
    return ErrorCode(ErrorDomain::NtStatus, static_cast<std::uint32_t>(status), {});
}

ErrorCode ErrorCode::fromLastError() noexcept {
    return ErrorCode(ErrorDomain::Win32, ::GetLastError(), {});
}

ErrorCode ErrorCode::application(std::string message) noexcept {
    return ErrorCode(ErrorDomain::Application, 0, std::move(message));
}

std::string ErrorCode::message() const {
    switch (domain_) {
        case ErrorDomain::None:
            return "No error";
        case ErrorDomain::Win32:
            return system_error::formatWin32Message(code_);
        case ErrorDomain::NtStatus:
            return system_error::formatNtStatusMessage(static_cast<std::int32_t>(code_));
        case ErrorDomain::Application:
            return context_.empty() ? "Application error" : context_;
    }
    return "Unknown error";
}

}  // namespace wis