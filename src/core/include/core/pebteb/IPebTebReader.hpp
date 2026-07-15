#pragma once

// Abstraction over PEB/TEB reading (DIP).

#include <cstdint>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pebteb/PebInfo.hpp"
#include "core/pebteb/TebInfo.hpp"

namespace wis::core {

class IPebTebReader {
public:
    virtual ~IPebTebReader() = default;

    // Reads the PEB of `pid`, following pointers into the target address space
    // for the process parameters and loader data. Nested reads are best-effort:
    // a denied sub-read leaves those fields empty rather than failing the call.
    [[nodiscard]] virtual Result<PebInfo, ErrorCode> readPeb(std::uint32_t pid) const = 0;

    // Reads the TEB of `tid`, whose owning process is `pid` (the TEB lives in
    // that process's address space).
    [[nodiscard]] virtual Result<TebInfo, ErrorCode> readTeb(std::uint32_t pid,
                                                             std::uint32_t tid) const = 0;
};

}  // namespace wis::core
