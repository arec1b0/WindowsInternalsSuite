#pragma once

// Import Directory parser. Produces the list of imported modules and, per
// module, the symbols pulled in either by name (with hint) or by ordinal.

#include <cstdint>
#include <string>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core {

struct ImportedSymbol {
    bool byOrdinal = false;
    std::uint16_t ordinal = 0;  // valid when byOrdinal
    std::uint16_t hint = 0;     // valid when !byOrdinal
    std::string name;           // empty when byOrdinal
};

struct ImportedModule {
    std::string dllName;
    std::vector<ImportedSymbol> symbols;
};

// Parses the import table. An image with no imports yields an empty vector
// (a valid, non-error outcome).
[[nodiscard]] Result<std::vector<ImportedModule>, ErrorCode> parseImports(const PEFile& pe);

}  // namespace wis::core
