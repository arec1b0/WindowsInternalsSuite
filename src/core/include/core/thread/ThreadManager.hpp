#pragma once

// Concrete IThreadManager backed by the native API seam.

#include <cstdint>
#include <vector>

#include "core/thread/IThreadManager.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class ThreadManager final : public IThreadManager {
public:
    // `native` must outlive this manager (owned by the composition root).
    explicit ThreadManager(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<std::vector<ThreadInfo>, ErrorCode> snapshot(
        std::uint32_t pid) const override;
    [[nodiscard]] Result<ThreadDetails, ErrorCode> queryDetails(
        std::uint32_t tid) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
