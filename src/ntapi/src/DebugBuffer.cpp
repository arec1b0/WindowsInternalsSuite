#include "ntapi/DebugBuffer.hpp"

#include <utility>

#include "ntapi/NtDll.hpp"

namespace wis::ntapi {

DebugBuffer::DebugBuffer(void* buffer) noexcept : buffer_(buffer) {}

DebugBuffer::DebugBuffer(DebugBuffer&& other) noexcept
    : buffer_(std::exchange(other.buffer_, nullptr)) {}

DebugBuffer& DebugBuffer::operator=(DebugBuffer&& other) noexcept {
    if (this != &other) {
        release();
        buffer_ = std::exchange(other.buffer_, nullptr);
    }
    return *this;
}

DebugBuffer::~DebugBuffer() { release(); }

void DebugBuffer::release() noexcept {
    if (buffer_ != nullptr) {
        const auto destroy = NtDll::instance().destroyQueryDebugBuffer();
        if (destroy != nullptr) {
            destroy(buffer_);
        }
        buffer_ = nullptr;
    }
}

Result<DebugBuffer, ErrorCode> DebugBuffer::create() {
    const auto createFn = NtDll::instance().createQueryDebugBuffer();
    if (createFn == nullptr) {
        return makeUnexpected(ErrorCode::application("RtlCreateQueryDebugBuffer unavailable"));
    }
    // size 0 lets ntdll choose a default-sized buffer; no event pair needed.
    void* buffer = createFn(0, FALSE);
    if (buffer == nullptr) {
        return makeUnexpected(ErrorCode::application("Failed to allocate debug buffer"));
    }
    return DebugBuffer(buffer);
}

Result<std::vector<HeapSummary>, ErrorCode> queryProcessHeaps(std::uint32_t pid) {
    const auto queryFn = NtDll::instance().queryProcessDebugInformation();
    if (queryFn == nullptr) {
        return makeUnexpected(
            ErrorCode::application("RtlQueryProcessDebugInformation unavailable"));
    }

    auto bufferResult = DebugBuffer::create();
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }
    const DebugBuffer buffer = std::move(bufferResult).value();

    // Summary + heap list, WITHOUT per-entry enumeration (which would freeze the
    // target's heaps while every block is walked).
    const ULONG flags = debug_query::HeapSummary | debug_query::Heaps;
    const HANDLE targetPid = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid));

    const NtStatus status = queryFn(targetPid, flags, buffer.get());
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }

    const auto* info = static_cast<const RtlDebugInformation*>(buffer.get());
    const RtlProcessHeaps* heaps = info->Heaps;
    if (heaps == nullptr) {
        return std::vector<HeapSummary>{};  // no heap data returned
    }

    std::vector<HeapSummary> result;
    result.reserve(heaps->NumberOfHeaps);
    for (ULONG i = 0; i < heaps->NumberOfHeaps; ++i) {
        const RtlHeapInformation& heap = heaps->Heaps[i];
        HeapSummary summary;
        summary.baseAddress = reinterpret_cast<std::uint64_t>(heap.BaseAddress);
        summary.flags = heap.Flags;
        summary.bytesAllocated = heap.BytesAllocated;
        summary.bytesCommitted = heap.BytesCommitted;
        summary.numberOfEntries = heap.NumberOfEntries;
        summary.numberOfTags = heap.NumberOfTags;
        result.push_back(summary);
    }

    return result;
}

}  // namespace wis::ntapi
