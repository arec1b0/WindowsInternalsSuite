#pragma once

// Reusable "growing buffer" loop for native queries that report the required
// size via STATUS_INFO_LENGTH_MISMATCH (or BUFFER_TOO_SMALL / OVERFLOW).
// The Invoke callable performs one native call:
//     NtStatus invoke(void* buffer, LengthType length, LengthType* returnLength)
// Templated on LengthType because NtQueryVirtualMemory uses SIZE_T while the
// other queries use ULONG.

#include <cstddef>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "ntapi/NtStatus.hpp"

namespace wis::ntapi::detail {

template <typename LengthType, typename Invoke>
[[nodiscard]] Result<std::vector<std::byte>, ErrorCode> queryIntoGrowingBuffer(
    Invoke&& invoke, LengthType initialSize, int maxAttempts = 10) {
    LengthType size = initialSize;
    std::vector<std::byte> buffer;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        buffer.assign(static_cast<std::size_t>(size), std::byte{0});

        LengthType returnLength = 0;
        const NtStatus status = invoke(buffer.data(), size, &returnLength);

        if (ntSuccess(status)) {
            // Shrink to the actually-used length when the query reports it.
            if (returnLength > 0 && returnLength <= size) {
                buffer.resize(static_cast<std::size_t>(returnLength));
            }
            return buffer;
        }

        const bool tooSmall = status == status::InfoLengthMismatch ||
                              status == status::BufferOverflow ||
                              status == status::BufferTooSmall;
        if (!tooSmall) {
            return makeUnexpected(ErrorCode::fromNtStatus(status));
        }

        // Honor the size hint if larger; add 50% headroom to absorb races where
        // the system state grows between calls (e.g. process/handle tables).
        LengthType next = size * 2;
        if (returnLength > size) {
            next = returnLength + returnLength / 2;
        }
        size = next;
    }

    return makeUnexpected(
        ErrorCode::application("Native query did not converge on a buffer size"));
}

}  // namespace wis::ntapi::detail