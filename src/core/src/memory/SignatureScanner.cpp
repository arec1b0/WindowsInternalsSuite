#include "core/memory/SignatureScanner.hpp"

#include <cctype>
#include <utility>

namespace wis::core {
namespace {

// Converts a single hex digit to its value, or -1 if not a hex digit.
[[nodiscard]] int hexDigit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}  // namespace

SignatureScanner::SignatureScanner(std::vector<std::uint8_t> bytes,
                                   std::vector<bool> mask) noexcept
    : bytes_(std::move(bytes)), mask_(std::move(mask)) {}

Result<SignatureScanner, ErrorCode> SignatureScanner::compile(std::string_view pattern) {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> mask;

    std::size_t i = 0;
    while (i < pattern.size()) {
        if (isSpace(pattern[i])) {
            ++i;
            continue;
        }

        // Wildcard token: "?" or "??".
        if (pattern[i] == '?') {
            ++i;
            if (i < pattern.size() && pattern[i] == '?') {
                ++i;
            }
            bytes.push_back(0);
            mask.push_back(false);
            continue;
        }

        // Hex byte token: exactly two hex digits.
        const int high = hexDigit(pattern[i]);
        if (high < 0 || i + 1 >= pattern.size()) {
            return makeUnexpected(ErrorCode::application("Signature: malformed hex token"));
        }
        const int low = hexDigit(pattern[i + 1]);
        if (low < 0) {
            return makeUnexpected(ErrorCode::application("Signature: malformed hex token"));
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
        mask.push_back(true);
        i += 2;
    }

    if (bytes.empty()) {
        return makeUnexpected(ErrorCode::application("Signature: empty pattern"));
    }

    return SignatureScanner(std::move(bytes), std::move(mask));
}

std::vector<std::size_t> SignatureScanner::findAll(ByteSpan data) const {
    std::vector<std::size_t> matches;
    const std::size_t patternSize = bytes_.size();
    if (patternSize == 0 || data.size() < patternSize) {
        return matches;
    }

    const std::size_t last = data.size() - patternSize;
    for (std::size_t offset = 0; offset <= last; ++offset) {
        bool hit = true;
        for (std::size_t j = 0; j < patternSize; ++j) {
            if (mask_[j] && static_cast<std::uint8_t>(data[offset + j]) != bytes_[j]) {
                hit = false;
                break;
            }
        }
        if (hit) {
            matches.push_back(offset);
        }
    }
    return matches;
}

}  // namespace wis::core
