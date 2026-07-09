#include "common/text/HexFormat.hpp"

#include <format>

namespace wis::hex {
namespace {

constexpr std::size_t kBytesPerRow = 16;

char nibble(std::uint8_t value, bool uppercase) noexcept {
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    return digits[value & 0x0F];
}

}  // namespace

std::string toHexString(ByteSpan data, bool uppercase) {
    std::string result;
    result.reserve(data.size() * 2);
    for (const std::byte byte : data) {
        const auto value = static_cast<std::uint8_t>(byte);
        result.push_back(nibble(value >> 4, uppercase));
        result.push_back(nibble(value, uppercase));
    }
    return result;
}

std::string dump(ByteSpan data, std::uint64_t baseAddress) {
    std::string result;

    for (std::size_t offset = 0; offset < data.size(); offset += kBytesPerRow) {
        const std::size_t rowSize =
            (data.size() - offset < kBytesPerRow) ? data.size() - offset : kBytesPerRow;

        result += std::format("{:016X}  ", baseAddress + offset);

        // Hex columns (pad missing bytes on the final short row).
        for (std::size_t i = 0; i < kBytesPerRow; ++i) {
            if (i < rowSize) {
                const auto value = static_cast<std::uint8_t>(data[offset + i]);
                result += std::format("{:02X} ", value);
            } else {
                result += "   ";
            }
            if (i == 7) {
                result.push_back(' ');  // extra gap between the two 8-byte halves
            }
        }

        result += " ";

        // ASCII column: printable bytes as-is, others as '.'.
        for (std::size_t i = 0; i < rowSize; ++i) {
            const auto value = static_cast<std::uint8_t>(data[offset + i]);
            result.push_back((value >= 0x20 && value < 0x7F) ? static_cast<char>(value) : '.');
        }

        result.push_back('\n');
    }

    return result;
}

}  // namespace wis::hex