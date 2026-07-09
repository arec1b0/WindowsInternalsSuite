#pragma once

// Concrete IProcessManager backed by the native API seam.

#include <cstdint>
#include <vector>

#include "core/process/IProcessManager.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class ProcessManager final : public IProcessManager {
public:
    // `native` must outlive this manager (owned by the composition root).
    explicit ProcessManager(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<ProcessInfo>, ErrorCode> snapshot() const override;
    [[nodiscard]] Result<ProcessDetails, ErrorCode> queryDetails(
        std::uint32_t pid) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core