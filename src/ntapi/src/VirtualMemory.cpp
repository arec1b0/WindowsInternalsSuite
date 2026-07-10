#include "ntapi/VirtualMemory.hpp"

#include "ntapi/NtDll.hpp"
#include "ntapi/detail/QueryBuffer.hpp"

namespace wis::ntapi::memory {

Result<std::size_t, ErrorCode> read(HANDLE process, std::uint64_t address,
                                    MutableByteSpan buffer) {
    const auto fn = NtDll::instance().readVirtualMemory();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtReadVirtualMemory unavailable"));
    }
    if (buffer.empty()) {
        return std::size_t{0};
    }

    SIZE_T bytesRead = 0;
    const NtStatus status = fn(process, reinterpret_cast<PVOID>(address), buffer.data(),
                               buffer.size(), &bytesRead);

    if (ntSuccess(status)) {
        return static_cast<std::size_t>(bytesRead);
    }
    // A partial copy across a page boundary is still useful for a memory viewer.
    if (status == status::PartialCopy && bytesRead > 0) {
        return static_cast<std::size_t>(bytesRead);
    }
    return makeUnexpected(ErrorCode::fromNtStatus(status));
}

Result<std::size_t, ErrorCode> write(HANDLE process, std::uint64_t address,
                                     ByteSpan buffer) {
    const auto fn = NtDll::instance().writeVirtualMemory();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtWriteVirtualMemory unavailable"));
    }
    if (buffer.empty()) {
        return std::size_t{0};
    }

    SIZE_T bytesWritten = 0;
    // NtWriteVirtualMemory takes a non-const buffer pointer; the write path does
    // not mutate it, so a const_cast at this ABI boundary is safe.
    const NtStatus status =
        fn(process, reinterpret_cast<PVOID>(address),
           const_cast<void*>(static_cast<const void*>(buffer.data())), buffer.size(),
           &bytesWritten);

    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return static_cast<std::size_t>(bytesWritten);
}

Result<MEMORY_BASIC_INFORMATION, ErrorCode> queryRegion(HANDLE process,
                                                        std::uint64_t address) {
    const auto fn = NtDll::instance().queryVirtualMemory();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryVirtualMemory unavailable"));
    }

    MEMORY_BASIC_INFORMATION mbi{};
    SIZE_T returnLength = 0;
    const NtStatus status =
        fn(process, reinterpret_cast<PVOID>(address), MemoryInformationClass::Basic, &mbi,
           sizeof(mbi), &returnLength);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return mbi;
}

Result<std::wstring, ErrorCode> queryMappedFilename(HANDLE process, std::uint64_t address) {
    const auto fn = NtDll::instance().queryVirtualMemory();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryVirtualMemory unavailable"));
    }

    auto bufferResult = detail::queryIntoGrowingBuffer<SIZE_T>(
        [&](void* buffer, SIZE_T length, SIZE_T* returnLength) {
            return fn(process, reinterpret_cast<PVOID>(address),
                      MemoryInformationClass::MappedFilename, buffer, length, returnLength);
        },
        512);
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    const auto& buffer = bufferResult.value();
    if (buffer.size() < sizeof(UnicodeString)) {
        return std::wstring{};  // region is not file-backed
    }
    const auto* name = reinterpret_cast<const UnicodeString*>(buffer.data());
    return unicodeStringToWide(*name);
}

}  // namespace wis::ntapi::memory