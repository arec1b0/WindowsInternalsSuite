#pragma once

// Byte-pattern scanner supporting IDA-style masks, e.g. "48 8B ?? ?? C3".
// A token of "??" (or "?") is a wildcard that matches any byte. The scanner is
// pure and buffer-oriented so it can be unit-tested without any live process.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/util/ByteSpan.hpp"

namespace wis::core {

class SignatureScanner {
public:
    // Compiles a space-separated pattern into bytes + a match mask.
    // Fails on malformed tokens or an empty pattern.
    [[nodiscard]] static Result<SignatureScanner, ErrorCode> compile(std::string_view pattern);

    // Offsets within `data` where the pattern matches. Empty when no match.
    [[nodiscard]] std::vector<std::size_t> findAll(ByteSpan data) const;

    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }

private:
    SignatureScanner(std::vector<std::uint8_t> bytes, std::vector<bool> mask) noexcept;

    std::vector<std::uint8_t> bytes_;  // pattern bytes (wildcard slot = 0)
    std::vector<bool> mask_;           // true = must match, false = wildcard
};

}  // namespace wis::core
