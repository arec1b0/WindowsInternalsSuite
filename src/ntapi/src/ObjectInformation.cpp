#include "ntapi/ObjectInformation.hpp"

#include "ntapi/NtDll.hpp"
#include "ntapi/detail/QueryBuffer.hpp"

namespace wis::ntapi::object_info {
namespace {

// DUPLICATE_SAME_ACCESS is defined in winnt.h (value 0x2).
constexpr ULONG kDuplicateSameAccess = DUPLICATE_SAME_ACCESS;

}  // namespace

Result<std::wstring, ErrorCode> queryName(HANDLE handle) {
    const auto fn = NtDll::instance().queryObject();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryObject unavailable"));
    }

    // TODO(handle-explorer): isolate this call behind a worker thread + timeout
    // to defend against handles whose name query blocks.
    auto bufferResult = detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(handle, ObjectInformationClass::Name, buffer, length, returnLength);
        },
        512);
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    const auto& buffer = bufferResult.value();
    if (buffer.size() < sizeof(ObjectNameInformation)) {
        return std::wstring{};  // unnamed object
    }
    const auto* info = reinterpret_cast<const ObjectNameInformation*>(buffer.data());
    return unicodeStringToWide(info->Name);
}

Result<std::wstring, ErrorCode> queryType(HANDLE handle) {
    const auto fn = NtDll::instance().queryObject();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtQueryObject unavailable"));
    }

    auto bufferResult = detail::queryIntoGrowingBuffer<ULONG>(
        [&](void* buffer, ULONG length, ULONG* returnLength) {
            return fn(handle, ObjectInformationClass::Type, buffer, length, returnLength);
        },
        sizeof(ObjectTypeInformation) + 64);
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    const auto& buffer = bufferResult.value();
    if (buffer.size() < sizeof(ObjectTypeInformation)) {
        return makeUnexpected(ErrorCode::application("Object type buffer too small"));
    }
    const auto* info = reinterpret_cast<const ObjectTypeInformation*>(buffer.data());
    return unicodeStringToWide(info->TypeName);
}

Result<Handle, ErrorCode> duplicateIntoSelf(HANDLE sourceProcess, HANDLE sourceHandle) {
    const auto fn = NtDll::instance().duplicateObject();
    if (fn == nullptr) {
        return makeUnexpected(ErrorCode::application("NtDuplicateObject unavailable"));
    }

    HANDLE duplicate = nullptr;
    const NtStatus status = fn(sourceProcess, sourceHandle, ::GetCurrentProcess(),
                               &duplicate, 0, 0, kDuplicateSameAccess);
    if (!ntSuccess(status)) {
        return makeUnexpected(ErrorCode::fromNtStatus(status));
    }
    return Handle(duplicate);
}

}  // namespace wis::ntapi::object_info