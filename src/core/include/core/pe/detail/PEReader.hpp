#pragma once

// Bounds-checked reads over a PEFile's mapped bytes, addressed by RVA. Every
// helper copies out of the mapping into a local object (no aliasing/unaligned
// hazards) and validates the range first, so malformed input yields nullopt
// instead of undefined behavior. Shared by the import/export/resource parsers.

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>

#include "common/util/ByteSpan.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core::detail {

// Copies a trivially-copyable T from a file offset, if fully in range.
template <typename T>
[[nodiscard]] std::optional<T> readAtOffset(ByteSpan data, std::size_t offset) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "readAtOffset requires a POD type");
    if (offset > data.size() || data.size() - offset < sizeof(T)) {
        return std::nullopt;
    }
    T value{};
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return value;
}

// Copies a trivially-copyable T addressed by RVA.
template <typename T>
[[nodiscard]] std::optional<T> readAtRva(const PEFile& pe, std::uint32_t rva) noexcept {
    const auto offset = pe.rvaToOffset(rva);
    if (!offset) {
        return std::nullopt;
    }
    return readAtOffset<T>(pe.bytes(), *offset);
}

// Reads a NUL-terminated ASCII string at an RVA, bounded by maxLength and by the
// end of the mapped file. Returns nullopt only when the RVA itself is invalid.
[[nodiscard]] inline std::optional<std::string> readCStringAtRva(
    const PEFile& pe, std::uint32_t rva, std::size_t maxLength = 4096) {
    const auto offset = pe.rvaToOffset(rva);
    if (!offset) {
        return std::nullopt;
    }
    const ByteSpan data = pe.bytes();
    std::string result;
    for (std::size_t i = 0; i < maxLength; ++i) {
        const std::size_t pos = *offset + i;
        if (pos >= data.size()) {
            break;
        }
        const auto ch = static_cast<char>(data[pos]);
        if (ch == '\0') {
            break;
        }
        result.push_back(ch);
    }
    return result;
}

}  // namespace wis::core::detail
