#include "core/handle/ObjectTypeTable.hpp"

#include <windows.h>

#include <cstring>
#include <utility>

#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// OBJECT_TYPES_INFORMATION: ULONG NumberOfTypes; followed by NumberOfTypes
// OBJECT_TYPE_INFORMATION records. Each record is padded so the next one starts
// on a pointer boundary, and its TypeName buffer follows the struct inline.
struct ObjectTypesInformationHeader {
    ULONG NumberOfTypes;
};

// Rounds a size up to the next pointer-aligned boundary.
std::size_t alignUp(std::size_t value) noexcept {
    constexpr std::size_t alignment = sizeof(void*);
    return (value + (alignment - 1)) & ~(alignment - 1);
}

}  // namespace

ObjectTypeTable::ObjectTypeTable(
    std::unordered_map<std::uint16_t, std::wstring> map) noexcept
    : indexToName_(std::move(map)) {}

std::wstring ObjectTypeTable::nameFor(std::uint16_t typeIndex) const {
    const auto it = indexToName_.find(typeIndex);
    return (it != indexToName_.end()) ? it->second : std::wstring{};
}

Result<ObjectTypeTable, ErrorCode> ObjectTypeTable::build(const ntapi::INativeApi& native) {
    auto bufferResult = native.queryObjectTypes();
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }
    const std::vector<std::byte>& buffer = bufferResult.value();
    if (buffer.size() < sizeof(ObjectTypesInformationHeader)) {
        return makeUnexpected(ErrorCode::application("Object types buffer too small"));
    }

    const std::byte* base = buffer.data();
    const std::size_t total = buffer.size();

    ObjectTypesInformationHeader header{};
    std::memcpy(&header, base, sizeof(header));

    std::unordered_map<std::uint16_t, std::wstring> map;
    map.reserve(header.NumberOfTypes);

    // The first record starts after the header, pointer-aligned.
    std::size_t offset = alignUp(sizeof(ObjectTypesInformationHeader));

    for (ULONG i = 0; i < header.NumberOfTypes; ++i) {
        if (offset + sizeof(ntapi::ObjectTypeInformation) > total) {
            break;  // truncated; keep whatever was parsed
        }

        ntapi::ObjectTypeInformation record{};
        std::memcpy(&record, base + offset, sizeof(record));

        // The TypeName buffer lies immediately after the record within the same
        // allocation; its length is in TypeName.Length (bytes).
        const std::size_t nameOffset = offset + sizeof(ntapi::ObjectTypeInformation);
        const std::size_t nameBytes = record.TypeName.Length;

        std::wstring name;
        if (nameBytes > 0 && nameOffset + nameBytes <= total) {
            name.assign(reinterpret_cast<const wchar_t*>(base + nameOffset),
                        nameBytes / sizeof(wchar_t));
        }

        // TypeIndex is the authoritative mapping key on Win8.1+. When it is 0
        // (older behavior), fall back to the 1-based position in the table.
        const std::uint16_t index =
            (record.TypeIndex != 0) ? record.TypeIndex
                                    : static_cast<std::uint16_t>(i + 1);
        if (!name.empty()) {
            map.emplace(index, std::move(name));
        }

        // Advance past this record and its name buffer, pointer-aligned.
        offset = alignUp(nameOffset + record.TypeName.MaximumLength);
    }

    return ObjectTypeTable(std::move(map));
}

}  // namespace wis::core
