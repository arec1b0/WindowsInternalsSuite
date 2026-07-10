#pragma once

// Resource Directory parser. Produces the resource tree (Type -> Name/ID ->
// Language -> data leaf). Each node is either a directory (with children) or a
// data leaf (with an RVA/size/code page pointing at the resource bytes).

#include <cstdint>
#include <string>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core {

struct ResourceNode {
    bool hasName = false;   // true: identified by name; false: by numeric id
    std::wstring name;      // valid when hasName
    std::uint32_t id = 0;   // valid when !hasName

    bool isData = false;    // true: leaf; false: directory
    std::vector<ResourceNode> children;  // valid when !isData

    // Leaf payload (valid when isData).
    std::uint32_t dataRva = 0;
    std::uint32_t dataSize = 0;
    std::uint32_t codePage = 0;
};

// Well-known top-level resource type name (RT_ICON, RT_STRING, ...), or empty
// when the id has no standard mapping.
[[nodiscard]] std::string_view resourceTypeName(std::uint32_t typeId) noexcept;

// Parses the resource tree. An image with no resources yields an empty vector
// (a valid, non-error outcome).
[[nodiscard]] Result<std::vector<ResourceNode>, ErrorCode> parseResources(const PEFile& pe);

}  // namespace wis::core
