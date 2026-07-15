#pragma once

// Dynamic binding to ntdll.dll exports. Function pointers are resolved once at
// first access via a thread-safe function-local static. Unresolved exports
// remain null; callers must check before invoking.

#include <windows.h>

#include "ntapi/NtStatus.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::ntapi {

// --- Native API function-pointer signatures (NTAPI == __stdcall on x86;
//     single convention on x64). Buffers are passed as raw pointers. ---

using NtQuerySystemInformationFn =
    NtStatus(NTAPI*)(SystemInformationClass systemInformationClass,
                     PVOID systemInformation,
                     ULONG systemInformationLength,
                     ULONG* returnLength);

using NtQueryInformationProcessFn =
    NtStatus(NTAPI*)(HANDLE processHandle,
                     ProcessInformationClass processInformationClass,
                     PVOID processInformation,
                     ULONG processInformationLength,
                     ULONG* returnLength);

using NtQueryInformationThreadFn =
    NtStatus(NTAPI*)(HANDLE threadHandle,
                     ThreadInformationClass threadInformationClass,
                     PVOID threadInformation,
                     ULONG threadInformationLength,
                     ULONG* returnLength);

using NtReadVirtualMemoryFn =
    NtStatus(NTAPI*)(HANDLE processHandle,
                     PVOID baseAddress,
                     PVOID buffer,
                     SIZE_T bufferSize,
                     SIZE_T* numberOfBytesRead);

using NtWriteVirtualMemoryFn =
    NtStatus(NTAPI*)(HANDLE processHandle,
                     PVOID baseAddress,
                     PVOID buffer,
                     SIZE_T bufferSize,
                     SIZE_T* numberOfBytesWritten);

using NtQueryVirtualMemoryFn =
    NtStatus(NTAPI*)(HANDLE processHandle,
                     PVOID baseAddress,
                     MemoryInformationClass memoryInformationClass,
                     PVOID memoryInformation,
                     SIZE_T memoryInformationLength,
                     SIZE_T* returnLength);

using NtOpenProcessFn =
    NtStatus(NTAPI*)(PHANDLE processHandle,
                     ACCESS_MASK desiredAccess,
                     ObjectAttributes* objectAttributes,
                     ClientId* clientId);

using NtOpenThreadFn =
    NtStatus(NTAPI*)(PHANDLE threadHandle,
                     ACCESS_MASK desiredAccess,
                     ObjectAttributes* objectAttributes,
                     ClientId* clientId);

using NtQueryObjectFn =
    NtStatus(NTAPI*)(HANDLE handle,
                     ObjectInformationClass objectInformationClass,
                     PVOID objectInformation,
                     ULONG objectInformationLength,
                     ULONG* returnLength);

using NtDuplicateObjectFn =
    NtStatus(NTAPI*)(HANDLE sourceProcessHandle,
                     HANDLE sourceHandle,
                     HANDLE targetProcessHandle,
                     PHANDLE targetHandle,
                     ACCESS_MASK desiredAccess,
                     ULONG handleAttributes,
                     ULONG options);

using NtCloseFn = NtStatus(NTAPI*)(HANDLE handle);

// Debug-buffer heap/module query trio (ntdll). RtlCreateQueryDebugBuffer
// allocates an opaque buffer; RtlQueryProcessDebugInformation fills it for a
// target PID; RtlDestroyQueryDebugBuffer frees it.
using RtlCreateQueryDebugBufferFn = PVOID(NTAPI*)(ULONG size, BOOLEAN eventPair);

using RtlQueryProcessDebugInformationFn =
    NtStatus(NTAPI*)(HANDLE uniqueProcessId, ULONG flags, PVOID debugBuffer);

using RtlDestroyQueryDebugBufferFn = NtStatus(NTAPI*)(PVOID debugBuffer);

// RtlGetVersion reports the true OS version, unaffected by the manifest-based
// version shimming that GetVersionEx is subject to.
using RtlGetVersionFn = NtStatus(NTAPI*)(RTL_OSVERSIONINFOEXW* versionInformation);

// Resolved ntdll entry points. Accessed as an immutable singleton.
class NtDll {
public:
    [[nodiscard]] static const NtDll& instance();

    NtDll(const NtDll&) = delete;
    NtDll& operator=(const NtDll&) = delete;

    [[nodiscard]] NtQuerySystemInformationFn querySystemInformation() const noexcept {
        return querySystemInformation_;
    }
    [[nodiscard]] NtQueryInformationProcessFn queryInformationProcess() const noexcept {
        return queryInformationProcess_;
    }
    [[nodiscard]] NtQueryInformationThreadFn queryInformationThread() const noexcept {
        return queryInformationThread_;
    }
    [[nodiscard]] NtReadVirtualMemoryFn readVirtualMemory() const noexcept {
        return readVirtualMemory_;
    }
    [[nodiscard]] NtWriteVirtualMemoryFn writeVirtualMemory() const noexcept {
        return writeVirtualMemory_;
    }
    [[nodiscard]] NtQueryVirtualMemoryFn queryVirtualMemory() const noexcept {
        return queryVirtualMemory_;
    }
    [[nodiscard]] NtOpenProcessFn openProcess() const noexcept { return openProcess_; }
    [[nodiscard]] NtOpenThreadFn openThread() const noexcept { return openThread_; }
    [[nodiscard]] NtQueryObjectFn queryObject() const noexcept { return queryObject_; }
    [[nodiscard]] NtDuplicateObjectFn duplicateObject() const noexcept {
        return duplicateObject_;
    }
    [[nodiscard]] NtCloseFn close() const noexcept { return close_; }

    [[nodiscard]] RtlCreateQueryDebugBufferFn createQueryDebugBuffer() const noexcept {
        return createQueryDebugBuffer_;
    }
    [[nodiscard]] RtlQueryProcessDebugInformationFn queryProcessDebugInformation()
        const noexcept {
        return queryProcessDebugInformation_;
    }
    [[nodiscard]] RtlDestroyQueryDebugBufferFn destroyQueryDebugBuffer() const noexcept {
        return destroyQueryDebugBuffer_;
    }
    [[nodiscard]] RtlGetVersionFn getVersion() const noexcept { return getVersion_; }

private:
    NtDll();

    NtQuerySystemInformationFn querySystemInformation_ = nullptr;
    NtQueryInformationProcessFn queryInformationProcess_ = nullptr;
    NtQueryInformationThreadFn queryInformationThread_ = nullptr;
    NtReadVirtualMemoryFn readVirtualMemory_ = nullptr;
    NtWriteVirtualMemoryFn writeVirtualMemory_ = nullptr;
    NtQueryVirtualMemoryFn queryVirtualMemory_ = nullptr;
    NtOpenProcessFn openProcess_ = nullptr;
    NtOpenThreadFn openThread_ = nullptr;
    NtQueryObjectFn queryObject_ = nullptr;
    NtDuplicateObjectFn duplicateObject_ = nullptr;
    NtCloseFn close_ = nullptr;
    RtlCreateQueryDebugBufferFn createQueryDebugBuffer_ = nullptr;
    RtlQueryProcessDebugInformationFn queryProcessDebugInformation_ = nullptr;
    RtlDestroyQueryDebugBufferFn destroyQueryDebugBuffer_ = nullptr;
    RtlGetVersionFn getVersion_ = nullptr;
};

}  // namespace wis::ntapi