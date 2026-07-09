#pragma once

// Bounds-checked iterator over a NtQuerySystemInformation(SystemProcessInformation)
// buffer. Each process record is variable-length: a fixed header followed inline
// by NumberOfThreads SystemThreadInformation entries, chained via NextEntryOffset.
//
// The callback receives the process header and a view over its inline thread
// array. Returning false stops iteration early (used to short-circuit lookups).
//
// Every offset is validated against the buffer bounds before dereference, so a
// truncated or malformed buffer yields an error instead of undefined behavior.

#include <cstddef>
#include <span>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/util/ByteSpan.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::core::detail {

// Callback signature: bool(const ntapi::SystemProcessInformation&,
//                          std::span<const ntapi::SystemThreadInformation>)
// Return true to continue, false to stop.
template <typename Callback>
[[nodiscard]] Result<void, ErrorCode> forEachProcess(ByteSpan buffer, Callback&& callback) {
    const std::byte* base = buffer.data();
    const std::size_t total = buffer.size();
    std::size_t offset = 0;

    while (true) {
        if (offset + sizeof(ntapi::SystemProcessInformation) > total) {
            return makeUnexpected(
                ErrorCode::application("Process snapshot: truncated process header"));
        }

        const auto* process =
            reinterpret_cast<const ntapi::SystemProcessInformation*>(base + offset);

        const std::size_t threadCount = process->NumberOfThreads;
        const std::size_t threadsOffset = offset + sizeof(ntapi::SystemProcessInformation);

        if (threadCount > 0) {
            const std::size_t threadsBytes =
                threadCount * sizeof(ntapi::SystemThreadInformation);
            if (threadsOffset + threadsBytes > total) {
                return makeUnexpected(
                    ErrorCode::application("Process snapshot: thread array out of range"));
            }
        }

        const auto* threads =
            reinterpret_cast<const ntapi::SystemThreadInformation*>(base + threadsOffset);
        const std::span<const ntapi::SystemThreadInformation> threadSpan(threads, threadCount);

        if (!callback(*process, threadSpan)) {
            return Result<void, ErrorCode>{};
        }

        if (process->NextEntryOffset == 0) {
            break;  // last entry in the chain
        }

        const std::size_t next = offset + process->NextEntryOffset;
        if (next <= offset || next >= total) {
            return makeUnexpected(
                ErrorCode::application("Process snapshot: invalid NextEntryOffset"));
        }
        offset = next;
    }

    return Result<void, ErrorCode>{};
}

}  // namespace wis::core::detail