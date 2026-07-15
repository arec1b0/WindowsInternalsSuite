#pragma once

// Concrete IPebTebReader. Locates the PEB/TEB via the native information
// classes, then reads the structures out of the target process with
// NtReadVirtualMemory. All layouts used are partial and offset-asserted.

#include <cstdint>

#include "core/pebteb/IPebTebReader.hpp"
#include "ntapi/INativeApi.hpp"

namespace wis::core {

class PebTebReader final : public IPebTebReader {
public:
    // `native` must outlive this reader (owned by the composition root).
    explicit PebTebReader(const ntapi::INativeApi& native) noexcept;

    [[nodiscard]] Result<PebInfo, ErrorCode> readPeb(std::uint32_t pid) const override;
    [[nodiscard]] Result<TebInfo, ErrorCode> readTeb(std::uint32_t pid,
                                                     std::uint32_t tid) const override;

private:
    const ntapi::INativeApi& native_;
};

}  // namespace wis::core
