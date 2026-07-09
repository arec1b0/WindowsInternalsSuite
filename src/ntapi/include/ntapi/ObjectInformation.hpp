#pragma once

// Wrappers over NtQueryObject and NtDuplicateObject.

#include <string>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi::object_info {

// Object name (e.g. \Device\..., registry key path, section name).
// WARNING: NtQueryObject(Name) can block indefinitely on certain synchronous
// handles. Callers that enumerate untrusted handles must isolate this call
// (worker thread + timeout); see the Handle Explorer module.
[[nodiscard]] Result<std::wstring, ErrorCode> queryName(HANDLE handle);

// Object type name (e.g. "File", "Key", "Process", "Mutant", "Section").
[[nodiscard]] Result<std::wstring, ErrorCode> queryType(HANDLE handle);

// Duplicates a handle from a foreign process into the current process with the
// same granted access, so it can be inspected locally.
[[nodiscard]] Result<Handle, ErrorCode> duplicateIntoSelf(HANDLE sourceProcess,
                                                          HANDLE sourceHandle);

}  // namespace wis::ntapi::object_info