#include "common/raii/ScopedPrivilege.hpp"

#include <utility>

namespace wis {

ScopedPrivilege::ScopedPrivilege(Handle token, TOKEN_PRIVILEGES previous) noexcept
    : token_(std::move(token)), previous_(previous) {}

Result<ScopedPrivilege, ErrorCode> ScopedPrivilege::enable(const wchar_t* privilegeName) {
    HANDLE rawToken = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                           &rawToken) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }
    Handle token(rawToken);

    LUID luid{};
    if (::LookupPrivilegeValueW(nullptr, privilegeName, &luid) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    TOKEN_PRIVILEGES desired{};
    desired.PrivilegeCount = 1;
    desired.Privileges[0].Luid = luid;
    desired.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    TOKEN_PRIVILEGES previous{};
    DWORD previousLength = sizeof(previous);

    if (::AdjustTokenPrivileges(token.get(),
                                FALSE,
                                &desired,
                                sizeof(previous),
                                &previous,
                                &previousLength) == FALSE) {
        return makeUnexpected(ErrorCode::fromLastError());
    }

    // AdjustTokenPrivileges succeeds even when the privilege was not held;
    // ERROR_NOT_ALL_ASSIGNED signals that case explicitly.
    const DWORD lastError = ::GetLastError();
    if (lastError == ERROR_NOT_ALL_ASSIGNED) {
        return makeUnexpected(ErrorCode::fromWin32(lastError));
    }

    return ScopedPrivilege(std::move(token), previous);
}

ScopedPrivilege::~ScopedPrivilege() {
    if (token_.valid()) {
        // Restore the previous privilege state; ignore failure during teardown.
        ::AdjustTokenPrivileges(token_.get(), FALSE, &previous_, 0, nullptr, nullptr);
    }
}

}  // namespace wis