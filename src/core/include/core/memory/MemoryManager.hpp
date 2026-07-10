#pragma once

// Concrete IMemoryManager backed by the native API seam. Read-only: this class
// never writes target memory. Each public call opens the process with the
// minimal access it needs and closes it via RAII on return.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/memory/IMemoryManager.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class MemoryManager final : public IMemoryManager {
public:
    // `native` must outlive this manager (owned by the composition root).
    explicit MemoryManager(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<MemoryRegion>, ErrorCode> queryMap(
        std::uint32_t pid) const override;
    [[nodiscard]] Result<std::vector<std::byte>, ErrorCode> read(
        std::uint32_t pid, std::uint64_t address, std::size_t size) const override;
    [[nodiscard]] Result<std::vector<SignatureMatch>, ErrorCode> scan(
        std::uint32_t pid, std::string_view pattern) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
