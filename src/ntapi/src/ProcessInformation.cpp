#include "ntapi/ProcessInformation.hpp"

#include "ntapi/NtDll.hpp"
#include "ntapi/detail/QueryBuffer.hpp"

namespace wis::ntapi::process_info {

Result<Handle, ErrorCode> open(std::uint32_t pid, ACCESS_MASK desiredAccess) {
    const auto fn = NtDll::instance().openProcess();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtOpenProcess unavailable"));
    }

    ObjectAttributes attributes{};
    attributes.Length = static_cast<ULONG>(sizeof(attributes));

    ClientId clientId{};
    clientId.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid));
    clientId.UniqueThread = nullptr;

    HANDLE handle = nullptr;
    const NtStatus status = fn(&handle, desiredAccess, &attributes, &clientId);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return Handle(handle);
}

Result<ProcessBasicInformation, ErrorCode> queryBasic(HANDLE process) {
    const auto fn = NtDll::instance().queryInformationProcess();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryInformationProcess unavailable"));
    }

    ProcessBasicInformation info{};
    ULONG returnLength = 0;
    const NtStatus status = fn(process, ProcessInformationClass::Basic, &info,
                               static_cast<ULONG>(sizeof(info)), &returnLength);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return info;
}

Result<std::wstring, ErrorCode> queryCommandLine(HANDLE process) {
    const auto fn = NtDll::instance().queryInformationProcess();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryInformationProcess unavailable"));
    }

    auto bufferResult = detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(process, ProcessInformationClass::CommandLine, buffer, length,
                      returnLength);
        },
        512);
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    const auto& buffer = bufferResult.value();
    if (buffer.size() < sizeof(ProcessCommandLineInformation)) {
        return std::wstring{};  // no command line available
    }
    const auto* info =
        reinterpret_cast<const ProcessCommandLineInformation*>(buffer.data());
    return unicodeStringToWide(info->CommandLine);
}

Result<std::wstring, ErrorCode> queryImageName(HANDLE process) {
    const auto fn = NtDll::instance().queryInformationProcess();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryInformationProcess unavailable"));
    }

    auto bufferResult = detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(process, ProcessInformationClass::ImageFileName, buffer, length,
                      returnLength);
        },
        512);
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    const auto& buffer = bufferResult.value();
    if (buffer.size() < sizeof(UnicodeString)) {
        return std::wstring{};
    }
    const auto* name = reinterpret_cast<const UnicodeString*>(buffer.data());
    return unicodeStringToWide(*name);
}

}  // namespace wis::ntapi::process_info