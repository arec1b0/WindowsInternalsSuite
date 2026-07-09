#include "common/text/Encoding.hpp"

#include <windows.h>

#include <limits>

namespace wis::encoding {
namespace {

// Guards against int overflow when passing sizes to the Win32 conversion APIs.
[[nodiscard]] bool fitsInInt(std::size_t value) noexcept {
    return value <= static_cast<std::size_t>((std::numeric_limits<int>::max)());
}

}  // namespace

std::string wideToUtf8(std::wstring_view wide) {
    if (wide.empty() || !fitsInInt(wide.size())) {
        return {};
    }
    const int sourceLength = static_cast<int>(wide.size());

    const int required = ::WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), sourceLength, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), sourceLength, result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(std::string_view utf8) {
    if (utf8.empty() || !fitsInInt(utf8.size())) {
        return {};
    }
    const int sourceLength = static_cast<int>(utf8.size());

    const int required =
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), sourceLength, nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    ::MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), sourceLength, result.data(), required);
    return result;
}

}  // namespace wis::encoding