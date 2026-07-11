#pragma once

// Concrete IHeapInspector backed by the native debug-buffer heap query.

#include <cstdint>
#include <vector>

#include "core/heap/IHeapInspector.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class HeapInspector final : public IHeapInspector {
public:
    explicit HeapInspector(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<HeapInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
