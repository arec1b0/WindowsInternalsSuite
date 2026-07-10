#include "core/pe/PEImports.hpp"

#include <windows.h>

#include <utility>

#include "core/pe/detail/PEReader.hpp"

namespace wis::core {
namespace {

constexpr std::size_t kMaxDescriptors = 4096;   // guard against runaway tables
constexpr std::size_t kMaxThunks = 65536;       // per-module symbol cap

// Reads one thunk value (4 or 8 bytes) and classifies it. Returns false when
// the thunk cannot be read (out of range).
struct ThunkValue {
    std::uint64_t raw = 0;
    bool byOrdinal = false;
    std::uint16_t ordinal = 0;
    std::uint32_t nameRva = 0;  // IMAGE_IMPORT_BY_NAME RVA when !byOrdinal
};

[[nodiscard]] bool readThunk(const PEFile& pe, std::uint32_t thunkRva, bool is64,
                             ThunkValue& out) {
    if (is64) {
        auto value = detail::readAtRva<std::uint64_t>(pe, thunkRva);
        if (!value) {
            return false;
        }
        out.raw = *value;
        if ((out.raw & IMAGE_ORDINAL_FLAG64) != 0) {
            out.byOrdinal = true;
            out.ordinal = static_cast<std::uint16_t>(out.raw & 0xFFFFU);
        } else {
            out.nameRva = static_cast<std::uint32_t>(out.raw & 0x7FFFFFFFU);
        }
    } else {
        auto value = detail::readAtRva<std::uint32_t>(pe, thunkRva);
        if (!value) {
            return false;
        }
        out.raw = *value;
        if ((static_cast<std::uint32_t>(out.raw) & IMAGE_ORDINAL_FLAG32) != 0) {
            out.byOrdinal = true;
            out.ordinal = static_cast<std::uint16_t>(out.raw & 0xFFFFU);
        } else {
            out.nameRva = static_cast<std::uint32_t>(out.raw & 0x7FFFFFFFU);
        }
    }
    return true;
}

// Resolves an imported symbol from a name-table thunk (IMAGE_IMPORT_BY_NAME).
ImportedSymbol resolveNamedSymbol(const PEFile& pe, std::uint32_t importByNameRva) {
    ImportedSymbol symbol;
    symbol.byOrdinal = false;

    // IMAGE_IMPORT_BY_NAME: WORD Hint; CHAR Name[]; (Hint precedes the string).
    if (auto hint = detail::readAtRva<std::uint16_t>(pe, importByNameRva)) {
        symbol.hint = *hint;
    }
    if (auto name = detail::readCStringAtRva(pe, importByNameRva + sizeof(std::uint16_t))) {
        symbol.name = std::move(*name);
    }
    return symbol;
}

}  // namespace

Result<std::vector<ImportedModule>, ErrorCode> parseImports(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return std::vector<ImportedModule>{};  // no imports
    }

    const bool is64 = pe.is64Bit();
    std::vector<ImportedModule> modules;

    for (std::size_t index = 0; index < kMaxDescriptors; ++index) {
        const std::uint32_t descriptorRva =
            directory->virtualAddress +
            static_cast<std::uint32_t>(index * sizeof(IMAGE_IMPORT_DESCRIPTOR));

        auto descriptor = detail::readAtRva<IMAGE_IMPORT_DESCRIPTOR>(pe, descriptorRva);
        if (!descriptor) {
            break;  // ran off the mapped range; stop gracefully
        }

        // The descriptor array is terminated by an all-zero entry. Name == 0
        // (with no thunks) marks the end.
        if (descriptor->Name == 0 && descriptor->FirstThunk == 0 &&
            descriptor->OriginalFirstThunk == 0) {
            break;
        }

        ImportedModule module;
        if (auto dllName = detail::readCStringAtRva(pe, descriptor->Name)) {
            module.dllName = std::move(*dllName);
        }

        // Prefer the Import Lookup Table (OriginalFirstThunk); fall back to the
        // IAT (FirstThunk) for bound imports where the ILT is absent.
        std::uint32_t thunkArrayRva = descriptor->OriginalFirstThunk;
        if (thunkArrayRva == 0) {
            thunkArrayRva = descriptor->FirstThunk;
        }
        if (thunkArrayRva == 0) {
            modules.push_back(std::move(module));
            continue;
        }

        const std::uint32_t thunkSize =
            is64 ? sizeof(std::uint64_t) : sizeof(std::uint32_t);

        for (std::size_t t = 0; t < kMaxThunks; ++t) {
            const std::uint32_t thunkRva =
                thunkArrayRva + static_cast<std::uint32_t>(t * thunkSize);

            ThunkValue thunk;
            if (!readThunk(pe, thunkRva, is64, thunk)) {
                break;
            }
            if (thunk.raw == 0) {
                break;  // null thunk terminates the array
            }

            if (thunk.byOrdinal) {
                ImportedSymbol symbol;
                symbol.byOrdinal = true;
                symbol.ordinal = thunk.ordinal;
                module.symbols.push_back(std::move(symbol));
            } else {
                module.symbols.push_back(resolveNamedSymbol(pe, thunk.nameRva));
            }
        }

        modules.push_back(std::move(module));
    }

    return modules;
}

}  // namespace wis::core
