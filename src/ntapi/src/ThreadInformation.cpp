#include "ntapi/ThreadInformation.hpp"

#include "ntapi/NtDll.hpp"

namespace wis::ntapi::thread_info {

Result<ThreadBasicInformation, ErrorCode> queryBasic(HANDLE thread) {
    const auto fn = NtDll::instance().queryInformationThread();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryInformationThread unavailable"));
    }

    ThreadBasicInformation info{};
    ULONG returnLength = 0;
    const NtStatus status = fn(thread, ThreadInformationClass::Basic, &info,
                               static_cast<ULONG>(sizeof(info)), &returnLength);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return info;
}

Result<void*, ErrorCode> queryWin32StartAddress(HANDLE thread) {
    const auto fn = NtDll::instance().queryInformationThread();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryInformationThread unavailable"));
    }

    PVOID startAddress = nullptr;
    ULONG returnLength = 0;
    const NtStatus status = fn(thread, ThreadInformationClass::Win32StartAddress,
                               &startAddress, static_cast<ULONG>(sizeof(startAddress)),
                               &returnLength);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return startAddress;
}

}  // namespace wis::ntapi::thread_info