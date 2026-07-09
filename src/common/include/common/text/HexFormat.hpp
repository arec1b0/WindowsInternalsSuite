#pragma once

// Hex formatting helpers for the Hex Viewer and memory/PE dumps.

#include <cstdint>
#include <string>

#include "common/util/ByteSpan.hpp"

namespace wis::hex {

// Contiguous hex string, e.g. {0xDE,0xAD} -> "DEAD" (no separators).
[[nodiscard]] std::string toHexString(ByteSpan data, bool uppercase = true);

// Canonical dump: "<offset>  <16 hex bytes>  <ascii>", one row per 16 bytes.
// `baseAddress` is added to the row offset (for absolute VA display).
[[nodiscard]] std::string dump(ByteSpan data, std::uint64_t baseAddress = 0);

}  // namespace wis::hex