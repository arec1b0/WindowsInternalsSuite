#pragma once

// Export Directory parser. Produces the module's export name plus each exported
// symbol: its ordinal, optional name, and either a target RVA or a forwarder
// string (e.g. "NTDLL.RtlAllocateHeap").

#include <cstdint>
#include <string>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core {

struct ExportedSymbol {
    std::uint32_t ordinal = 0;      // absolute ordinal (Base + index)
    bool hasName = false;
    std::string name;               // valid when hasName
    bool isForwarder = false;
    std::uint32_t functionRva = 0;  // valid when !isForwarder
    std::string forwarderTarget;    // valid when isForwarder
};

struct ExportInfo {
    std::string moduleName;
    std::uint32_t ordinalBase = 0;
    std::vector<ExportedSymbol> symbols;
};

// Parses the export table. An image with no exports yields an ExportInfo with
// an empty symbol list (a valid, non-error outcome).
[[nodiscard]] Result<ExportInfo, ErrorCode> parseExports(const PEFile& pe);

}  // namespace wis::core
