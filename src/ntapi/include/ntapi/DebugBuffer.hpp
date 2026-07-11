#pragma once

// RAII wrapper over an ntdll debug buffer, plus a typed heap query built on it.
// The buffer is allocated by RtlCreateQueryDebugBuffer and must be released by
// RtlDestroyQueryDebugBuffer; this type guarantees that pairing.

#include <windows.h>

#include <cstdint>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi {

// Owns a debug buffer for the lifetime of the object. Move-only.
class DebugBuffer {
public:
    [[nodiscard]] static Result<DebugBuffer, ErrorCode> create();

    DebugBuffer(const DebugBuffer&) = delete;
    DebugBuffer& operator=(const DebugBuffer&) = delete;
    DebugBuffer(DebugBuffer&& other) noexcept;
    DebugBuffer& operator=(DebugBuffer&& other) noexcept;
    ~DebugBuffer();

    [[nodiscard]] void* get() const noexcept { return buffer_; }
    [[nodiscard]] bool valid() const noexcept { return buffer_ != nullptr; }

private:
    explicit DebugBuffer(void* buffer) noexcept;
    void release() noexcept;

    void* buffer_ = nullptr;
};

// Flat, owned copy of one heap's summary (decoupled from the debug buffer's
// lifetime so callers can keep it after the buffer is destroyed).
struct HeapSummary {
    std::uint64_t baseAddress = 0;
    std::uint32_t flags = 0;
    std::uint64_t bytesAllocated = 0;
    std::uint64_t bytesCommitted = 0;
    std::uint32_t numberOfEntries = 0;   // allocated block count
    std::uint32_t numberOfTags = 0;
};

// Queries per-heap summaries for `pid`. Uses HEAP_SUMMARY | HEAPS only (no
// per-entry dump), so the target's heaps are not frozen for block enumeration.
[[nodiscard]] Result<std::vector<HeapSummary>, ErrorCode> queryProcessHeaps(std::uint32_t pid);

}  // namespace wis::ntapi
