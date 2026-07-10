#pragma once

// Abstraction over read-only virtual-memory inspection (DIP).

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/memory/MemoryRegion.hpp"

namespace wis::core {

// One signature match: absolute address in the target process.
struct SignatureMatch {
    std::uint64_t address = 0;
};

class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;

    // Full region map of the process, ordered by ascending base address.
    [[nodiscard]] virtual Result<std::vector<MemoryRegion>, ErrorCode> queryMap(
        std::uint32_t pid) const = 0;

    // Reads up to `size` bytes at `address`. May return fewer bytes than
    // requested (partial reads across page boundaries are reported, not failed).
    [[nodiscard]] virtual Result<std::vector<std::byte>, ErrorCode> read(
        std::uint32_t pid, std::uint64_t address, std::size_t size) const = 0;

    // Scans all committed, readable regions for a byte signature.
    [[nodiscard]] virtual Result<std::vector<SignatureMatch>, ErrorCode> scan(
        std::uint32_t pid, std::string_view pattern) const = 0;
};

}  // namespace wis::core
