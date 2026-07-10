#pragma once

// Domain model for one open handle, sourced from the system handle snapshot.

#include <cstdint>
#include <string>

namespace wis::core {

struct HandleInfo {
    std::uint32_t ownerPid = 0;
    std::uint64_t handleValue = 0;     // the handle number within the owner
    std::uint64_t objectAddress = 0;   // kernel object pointer
    std::uint16_t typeIndex = 0;       // snapshot type index (-> ObjectTypeTable)
    std::wstring typeName;             // resolved from the type table
    std::uint32_t grantedAccess = 0;   // ACCESS_MASK bits
    std::uint32_t attributes = 0;      // OBJ_INHERIT, OBJ_PROTECT_CLOSE, ...
};

// One resolved object name (looked up on demand, may time out).
struct HandleObjectName {
    bool resolved = false;
    bool timedOut = false;
    std::wstring name;
};

}  // namespace wis::core
