#pragma once

// DIP seam over the native API. Core modules depend on this interface, never on
// ntdll directly, so they can be unit-tested against a fake implementation.
// The concrete implementation is hidden behind createNativeApi().

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "common/raii/UniqueHandle.hpp"
#include "common/util/ByteSpan.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi {

class INativeApi {
public:
    virtual ~INativeApi() = default;

    // --- System-wide enumeration (owned variable-length buffers) ---
    [[nodiscard]] virtual Result<std::vector<std::byte>, ErrorCode>
    queryProcessSnapshot() const = 0;

    [[nodiscard]] virtual Result<std::vector<std::byte>, ErrorCode>
    queryHandleSnapshot() const = 0;

    [[nodiscard]] virtual Result<SystemBasicInformation, ErrorCode>
    querySystemBasicInformation() const = 0;

    // --- Process ---
    [[nodiscard]] virtual Result<Handle, ErrorCode> openProcess(
        std::uint32_t pid, ACCESS_MASK desiredAccess) const = 0;

    [[nodiscard]] virtual Result<ProcessBasicInformation, ErrorCode>
    queryProcessBasicInformation(HANDLE process) const = 0;

    [[nodiscard]] virtual Result<std::wstring, ErrorCode> queryProcessCommandLine(
        HANDLE process) const = 0;

    [[nodiscard]] virtual Result<std::wstring, ErrorCode> queryProcessImageName(
        HANDLE process) const = 0;

    // --- Thread ---
    [[nodiscard]] virtual Result<ThreadBasicInformation, ErrorCode>
    queryThreadBasicInformation(HANDLE thread) const = 0;

    [[nodiscard]] virtual Result<void*, ErrorCode> queryThreadStartAddress(
        HANDLE thread) const = 0;

    // --- Virtual memory ---
    [[nodiscard]] virtual Result<std::size_t, ErrorCode> readMemory(
        HANDLE process, std::uint64_t address, MutableByteSpan buffer) const = 0;

    [[nodiscard]] virtual Result<std::size_t, ErrorCode> writeMemory(
        HANDLE process, std::uint64_t address, ByteSpan buffer) const = 0;

    [[nodiscard]] virtual Result<std::wstring, ErrorCode> queryMappedFilename(
        HANDLE process, std::uint64_t address) const = 0;

    // --- Objects / handles ---
    [[nodiscard]] virtual Result<std::wstring, ErrorCode> queryObjectName(
        HANDLE handle) const = 0;

    [[nodiscard]] virtual Result<std::wstring, ErrorCode> queryObjectType(
        HANDLE handle) const = 0;

    [[nodiscard]] virtual Result<Handle, ErrorCode> duplicateHandle(
        HANDLE sourceProcess, HANDLE sourceHandle) const = 0;
};

// Factory for the production implementation (delegates to ntdll wrappers).
[[nodiscard]] std::unique_ptr<INativeApi> createNativeApi();

}  // namespace wis::ntapi