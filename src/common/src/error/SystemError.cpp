#include "common/error/SystemError.hpp"

#include <windows.h>

#include "common/raii/UniqueLocalMem.hpp"
#include "common/text/Encoding.hpp"

namespace wis::system_error {
namespace {

// Shared FormatMessage wrapper. `moduleHandle` is nullptr for system messages
// or a loaded module (ntdll) for NTSTATUS lookups.
std::string formatMessage(HMODULE moduleHandle, DWORD messageId, DWORD flags) {
    LPWSTR buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        flags | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        moduleHandle,
        messageId,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    // Ownership of the FormatMessage allocation via RAII (LocalFree).
    UniqueLocalMem<LPWSTR> owned(buffer);

    if (length == 0 || buffer == nullptr) {
        return "Unknown error";
    }

    std::wstring wide(buffer, length);
    // Trim trailing CR/LF that FormatMessage appends.
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n')) {
        wide.pop_back();
    }
    return encoding::wideToUtf8(wide);
}

}  // namespace

std::string formatWin32Message(std::uint32_t code) {
    return formatMessage(nullptr, static_cast<DWORD>(code), FORMAT_MESSAGE_FROM_SYSTEM);
}

std::string formatNtStatusMessage(std::int32_t status) {
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return "Unknown NTSTATUS";
    }
    return formatMessage(ntdll, static_cast<DWORD>(status), FORMAT_MESSAGE_FROM_HMODULE);
}

}  // namespace wis::system_error