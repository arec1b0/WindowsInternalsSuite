#include "core/pe/PEResources.hpp"

#include <windows.h>

#include <utility>

#include "core/pe/detail/PEReader.hpp"

namespace wis::core {
namespace {

// The resource tree is conventionally three levels deep (type/name/language);
// allow a little slack but cap recursion to defend against malformed files
// whose subdirectory offsets form a cycle.
constexpr int kMaxResourceDepth = 8;
constexpr std::size_t kMaxEntriesPerLevel = 8192;

// All intra-resource offsets are relative to the resource directory base RVA.
struct ResourceContext {
    const PEFile* pe = nullptr;
    std::uint32_t baseRva = 0;  // RVA of the resource data directory
};

// Reads the length-prefixed UTF-16 name (IMAGE_RESOURCE_DIR_STRING_U) at an
// offset relative to the resource base.
std::wstring readResourceName(const ResourceContext& ctx, std::uint32_t offset) {
    const std::uint32_t rva = ctx.baseRva + offset;
    auto length = detail::readAtRva<std::uint16_t>(*ctx.pe, rva);
    if (!length || *length == 0) {
        return {};
    }

    std::wstring name;
    name.reserve(*length);
    for (std::uint16_t i = 0; i < *length; ++i) {
        const std::uint32_t charRva =
            rva + sizeof(std::uint16_t) + i * sizeof(wchar_t);
        auto ch = detail::readAtRva<wchar_t>(*ctx.pe, charRva);
        if (!ch) {
            break;
        }
        name.push_back(*ch);
    }
    return name;
}

// Forward declaration for the recursive directory walk.
std::vector<ResourceNode> parseDirectory(const ResourceContext& ctx,
                                         std::uint32_t directoryOffset, int depth);

// Builds a leaf node from an IMAGE_RESOURCE_DATA_ENTRY at the given offset.
ResourceNode parseDataLeaf(const ResourceContext& ctx, std::uint32_t dataOffset) {
    ResourceNode node;
    node.isData = true;

    const std::uint32_t rva = ctx.baseRva + dataOffset;
    if (auto entry = detail::readAtRva<IMAGE_RESOURCE_DATA_ENTRY>(*ctx.pe, rva)) {
        // OffsetToData here is an image-relative RVA (not resource-relative).
        node.dataRva = entry->OffsetToData;
        node.dataSize = entry->Size;
        node.codePage = entry->CodePage;
    }
    return node;
}

std::vector<ResourceNode> parseDirectory(const ResourceContext& ctx,
                                         std::uint32_t directoryOffset, int depth) {
    std::vector<ResourceNode> nodes;
    if (depth > kMaxResourceDepth) {
        return nodes;
    }

    const std::uint32_t dirRva = ctx.baseRva + directoryOffset;
    auto header = detail::readAtRva<IMAGE_RESOURCE_DIRECTORY>(*ctx.pe, dirRva);
    if (!header) {
        return nodes;
    }

    const std::size_t entryCount =
        static_cast<std::size_t>(header->NumberOfNamedEntries) +
        static_cast<std::size_t>(header->NumberOfIdEntries);
    if (entryCount > kMaxEntriesPerLevel) {
        return nodes;
    }

    const std::uint32_t entriesRva = dirRva + sizeof(IMAGE_RESOURCE_DIRECTORY);
    for (std::size_t i = 0; i < entryCount; ++i) {
        const std::uint32_t entryRva =
            entriesRva + static_cast<std::uint32_t>(i * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
        auto entry = detail::readAtRva<IMAGE_RESOURCE_DIRECTORY_ENTRY>(*ctx.pe, entryRva);
        if (!entry) {
            break;
        }

        ResourceNode node;

        // Name field: high bit set -> offset to a UTF-16 name; else numeric id.
        if ((entry->Name & 0x80000000U) != 0) {
            node.hasName = true;
            node.name = readResourceName(ctx, entry->Name & 0x7FFFFFFFU);
        } else {
            node.hasName = false;
            node.id = entry->Name & 0xFFFFU;
        }

        // OffsetToData field: high bit set -> subdirectory; else data leaf.
        if ((entry->OffsetToData & 0x80000000U) != 0) {
            const std::uint32_t subOffset = entry->OffsetToData & 0x7FFFFFFFU;
            node.isData = false;
            node.children = parseDirectory(ctx, subOffset, depth + 1);
        } else {
            node = parseDataLeaf(ctx, entry->OffsetToData);
            // Restore identity fields overwritten by the leaf constructor.
            if ((entry->Name & 0x80000000U) != 0) {
                node.hasName = true;
                node.name = readResourceName(ctx, entry->Name & 0x7FFFFFFFU);
            } else {
                node.hasName = false;
                node.id = entry->Name & 0xFFFFU;
            }
        }

        nodes.push_back(std::move(node));
    }

    return nodes;
}

}  // namespace

std::string_view resourceTypeName(std::uint32_t typeId) noexcept {
    switch (typeId) {
        case 1:  return "RT_CURSOR";
        case 2:  return "RT_BITMAP";
        case 3:  return "RT_ICON";
        case 4:  return "RT_MENU";
        case 5:  return "RT_DIALOG";
        case 6:  return "RT_STRING";
        case 7:  return "RT_FONTDIR";
        case 8:  return "RT_FONT";
        case 9:  return "RT_ACCELERATOR";
        case 10: return "RT_RCDATA";
        case 11: return "RT_MESSAGETABLE";
        case 12: return "RT_GROUP_CURSOR";
        case 14: return "RT_GROUP_ICON";
        case 16: return "RT_VERSION";
        case 22: return "RT_ANIICON";
        case 24: return "RT_MANIFEST";
        default: return {};
    }
}

Result<std::vector<ResourceNode>, ErrorCode> parseResources(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_RESOURCE);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return std::vector<ResourceNode>{};  // no resources
    }

    ResourceContext ctx;
    ctx.pe = &pe;
    ctx.baseRva = directory->virtualAddress;

    // The root directory sits at offset 0 relative to the resource base.
    return parseDirectory(ctx, 0, 0);
}

}  // namespace wis::core
