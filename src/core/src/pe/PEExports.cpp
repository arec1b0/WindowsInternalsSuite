#include "core/pe/PEExports.hpp"

#include <windows.h>

#include <unordered_map>
#include <utility>

#include "core/pe/detail/PEReader.hpp"

namespace wis::core {
namespace {

constexpr std::uint32_t kMaxExports = 1U << 20;  // sanity cap on table sizes

}  // namespace

Result<ExportInfo, ErrorCode> parseExports(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return ExportInfo{};  // no exports
    }

    const std::uint32_t exportDirRva = directory->virtualAddress;
    const std::uint32_t exportDirSize = directory->size;

    auto exportDir = detail::readAtRva<IMAGE_EXPORT_DIRECTORY>(pe, exportDirRva);
    if (!exportDir) {
        return makeUnexpected(ErrorCode::application("PE: unreadable export directory"));
    }

    ExportInfo info;
    info.ordinalBase = exportDir->Base;
    if (auto moduleName = detail::readCStringAtRva(pe, exportDir->Name)) {
        info.moduleName = std::move(*moduleName);
    }

    const std::uint32_t functionCount = exportDir->NumberOfFunctions;
    const std::uint32_t nameCount = exportDir->NumberOfNames;
    if (functionCount > kMaxExports || nameCount > kMaxExports) {
        return makeUnexpected(ErrorCode::application("PE: export table size out of range"));
    }

    // Build an index->name map from the Export Name Table (ENT) and the parallel
    // Name-Ordinal Table (NOT). NOT entries are biased indices into the EAT.
    std::unordered_map<std::uint32_t, std::string> indexToName;
    indexToName.reserve(nameCount);

    for (std::uint32_t i = 0; i < nameCount; ++i) {
        const std::uint32_t nameRvaSlot =
            exportDir->AddressOfNames + i * sizeof(std::uint32_t);
        const std::uint32_t ordinalSlot =
            exportDir->AddressOfNameOrdinals + i * sizeof(std::uint16_t);

        auto nameRva = detail::readAtRva<std::uint32_t>(pe, nameRvaSlot);
        auto eatIndex = detail::readAtRva<std::uint16_t>(pe, ordinalSlot);
        if (!nameRva || !eatIndex) {
            continue;
        }
        if (auto name = detail::readCStringAtRva(pe, *nameRva)) {
            indexToName.emplace(static_cast<std::uint32_t>(*eatIndex), std::move(*name));
        }
    }

    // Walk the Export Address Table (EAT). Each non-zero slot is an export; a
    // slot whose RVA falls inside the export directory is a forwarder.
    info.symbols.reserve(functionCount);
    for (std::uint32_t i = 0; i < functionCount; ++i) {
        const std::uint32_t eatSlot =
            exportDir->AddressOfFunctions + i * sizeof(std::uint32_t);
        auto functionRva = detail::readAtRva<std::uint32_t>(pe, eatSlot);
        if (!functionRva) {
            break;
        }
        if (*functionRva == 0) {
            continue;  // unused ordinal slot
        }

        ExportedSymbol symbol;
        symbol.ordinal = info.ordinalBase + i;

        if (auto it = indexToName.find(i); it != indexToName.end()) {
            symbol.hasName = true;
            symbol.name = it->second;
        }

        const bool forwarded = *functionRva >= exportDirRva &&
                               *functionRva < exportDirRva + exportDirSize;
        if (forwarded) {
            symbol.isForwarder = true;
            if (auto target = detail::readCStringAtRva(pe, *functionRva)) {
                symbol.forwarderTarget = std::move(*target);
            }
        } else {
            symbol.functionRva = *functionRva;
        }

        info.symbols.push_back(std::move(symbol));
    }

    return info;
}

}  // namespace wis::core
