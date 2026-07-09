#pragma once

// Enables a token privilege (e.g. SeDebugPrivilege) on the current process
// for the object's lifetime, restoring the previous state on destruction.
// Required for cross-process memory reads on protected processes.

#include <windows.h>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"

namespace wis {

class ScopedPrivilege {
public:
    // Attempts to enable `privilegeName` (e.g. SE_DEBUG_NAME) on the current
    // process token. Returns an error if the privilege is unavailable.
    [[nodiscard]] static Result<ScopedPrivilege, ErrorCode> enable(const wchar_t* privilegeName);

    ScopedPrivilege(const ScopedPrivilege&) = delete;
    ScopedPrivilege& operator=(const ScopedPrivilege&) = delete;

    ScopedPrivilege(ScopedPrivilege&&) noexcept = default;
    ScopedPrivilege& operator=(ScopedPrivilege&&) noexcept = default;

    ~ScopedPrivilege();

private:
    ScopedPrivilege(Handle token, TOKEN_PRIVILEGES previous) noexcept;

    Handle token_;
    TOKEN_PRIVILEGES previous_{};
};

}  // namespace wis