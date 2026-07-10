#pragma once

// Maps and parses the structural spine of a PE image: DOS/NT/Optional headers
// and the section table. Directory contents (imports/exports/resources) are
// parsed separately, on demand, by dedicated helpers that take a PEFile.

#include <string_view>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core {

class PEParser {
public:
    [[nodiscard]] static Result<PEFile, ErrorCode> parseFile(std::wstring_view path);
};

}  // namespace wis::core
