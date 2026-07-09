#pragma once

// UTF-16 <-> UTF-8 conversion. All internal string handling is UTF-8;
// conversion to wide happens only at the Win32 API boundary.

#include <string>
#include <string_view>

namespace wis::encoding {

[[nodiscard]] std::string wideToUtf8(std::wstring_view wide);
[[nodiscard]] std::wstring utf8ToWide(std::string_view utf8);

}  // namespace wis::encoding