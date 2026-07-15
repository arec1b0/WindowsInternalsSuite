#include "ntapi/SystemInformation.hpp"

#include "ntapi/NtDll.hpp"
#include "ntapi/detail/QueryBuffer.hpp"

namespace wis::ntapi::system_info {

Result<std::vector<std::byte>, ErrorCode> queryProcesses() {
    const auto fn = NtDll::instance().querySystemInformation();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQuerySystemInformation unavailable"));
    }
    // 128 KiB initial: comfortably fits a typical process+thread snapshot.
    return detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(SystemInformationClass::Process, buffer, length, returnLength);
        },
        0x20000);
}

Result<std::vector<std::byte>, ErrorCode> queryHandles() {
    const auto fn = NtDll::instance().querySystemInformation();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQuerySystemInformation unavailable"));
    }
    // 1 MiB initial: the system-wide handle table is large on a busy machine.
    return detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(SystemInformationClass::ExtendedHandle, buffer, length, returnLength);
        },
        0x100000);
}

Result<SystemBasicInformation, ErrorCode> queryBasic() {
    const auto fn = NtDll::instance().querySystemInformation();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQuerySystemInformation unavailable"));
    }

    SystemBasicInformation info{};
    ULONG returnLength = 0;
    const NtStatus status = fn(SystemInformationClass::Basic, &info,
                               static_cast<ULONG>(sizeof(info)), &returnLength);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return info;
}

Result<std::vector<std::byte>, ErrorCode> queryPagefiles() {
    const auto fn = NtDll::instance().querySystemInformation();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQuerySystemInformation unavailable"));
    }
    // A system with no pagefile returns STATUS_SUCCESS with a zero-length buffer;
    // 4 KiB initial comfortably fits the handful of pagefiles a system can have.
    return detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(SystemInformationClass::Pagefile, buffer, length, returnLength);
        },
        0x1000);
}

Result<RTL_OSVERSIONINFOEXW, ErrorCode> queryOsVersion() {
    const auto fn = NtDll::instance().getVersion();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("RtlGetVersion unavailable"));
    }

    RTL_OSVERSIONINFOEXW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    const NtStatus status = fn(&info);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return info;
}

}  // namespace wis::ntapi::system_info