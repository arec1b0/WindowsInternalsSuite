#pragma once

// Abstraction over address symbolization (DIP). A resolver instance is bound to
// one target process for its lifetime, because the underlying symbol handler
// keeps per-process state.

#include <cstdint>
#include <string>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"

namespace wis::core {

// One resolved address. Fields fill in progressively: a resolver may know the
// module but not the symbol (no PDB), or the symbol but not the line.
struct SymbolLocation {
    bool hasModule = false;
    std::wstring moduleName;        // e.g. "ntdll.dll"
    std::uint64_t moduleBase = 0;

    bool hasSymbol = false;
    std::string symbolName;         // undecorated when available
    std::uint64_t displacement = 0; // bytes past the symbol start

    bool hasLine = false;
    std::string fileName;
    std::uint32_t lineNumber = 0;
};

// Formats a location as "module!symbol+0x1234" / "module+0x1234" / "0x...".
[[nodiscard]] std::string formatSymbol(const SymbolLocation& location,
                                       std::uint64_t address);

class ISymbolResolver {
public:
    virtual ~ISymbolResolver() = default;

    // Resolves an address within the bound process. Never fails outright: an
    // unresolvable address yields a location with all flags false.
    [[nodiscard]] virtual SymbolLocation resolve(std::uint64_t address) const = 0;

    // True when the symbol handler initialized successfully for this process.
    [[nodiscard]] virtual bool ready() const noexcept = 0;
};

}  // namespace wis::core
