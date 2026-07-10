#pragma once

// Maps object type indices (as reported in the system handle snapshot) to their
// type names ("File", "Key", "Process", ...). Built once from
// NtQueryObject(ObjectTypesInformation) and reused across the whole listing,
// avoiding a per-handle duplicate-and-query round trip just to learn the type.

#include <cstdint>
#include <string>
#include <unordered_map>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class ObjectTypeTable {
public:
    // Builds the table from the native object-type information.
    [[nodiscard]] static Result<ObjectTypeTable, ErrorCode> build(
        const ntapi::INativeApi& native);

    // Type name for a snapshot type index, or an empty string when unknown.
    [[nodiscard]] std::wstring nameFor(std::uint16_t typeIndex) const;

    [[nodiscard]] bool empty() const noexcept { return indexToName_.empty(); }

private:
    explicit ObjectTypeTable(std::unordered_map<std::uint16_t, std::wstring> map) noexcept;

    std::unordered_map<std::uint16_t, std::wstring> indexToName_;
};

}  // namespace wis::core
