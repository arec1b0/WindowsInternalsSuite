#include "core/pebteb/PebTebReader.hpp"

#include <windows.h>

#include <cstring>
#include <utility>
#include <vector>

#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// Query + read the target address space.
constexpr ACCESS_MASK kProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ;
constexpr ACCESS_MASK kThreadAccess = THREAD_QUERY_LIMITED_INFORMATION;

// Upper bound on a remote UNICODE_STRING we are willing to pull in.
constexpr std::uint16_t kMaxRemoteStringBytes = 8192;

// Reads a trivially-copyable T from the target process at `address`.
template <typename T>
[[nodiscard]] bool readRemote(const ntapi::INativeApi& native, HANDLE process,
                              std::uint64_t address, T& out) {
    static_assert(std::is_trivially_copyable_v<T>, "readRemote requires a POD type");
    if (address == 0) {
        return false;
    }
    std::byte buffer[sizeof(T)]{};
    auto result = native.readMemory(process, address, MutableByteSpan(buffer, sizeof(T)));
    if (!result || result.value() != sizeof(T)) {
        return false;
    }
    std::memcpy(&out, buffer, sizeof(T));
    return true;
}

// Follows a remote UNICODE_STRING and copies its characters out of the target.
std::wstring readRemoteUnicodeString(const ntapi::INativeApi& native, HANDLE process,
                                     const ntapi::UnicodeString& descriptor) {
    if (descriptor.Buffer == nullptr || descriptor.Length == 0) {
        return {};
    }
    const std::uint16_t byteLength =
        (descriptor.Length > kMaxRemoteStringBytes) ? kMaxRemoteStringBytes : descriptor.Length;

    std::vector<std::byte> buffer(byteLength);
    auto result = native.readMemory(process,
                                    reinterpret_cast<std::uint64_t>(descriptor.Buffer),
                                    MutableByteSpan(buffer.data(), buffer.size()));
    if (!result || result.value() == 0) {
        return {};
    }
    // Use only what was actually read, truncated to whole wchar_t units.
    const std::size_t chars = result.value() / sizeof(wchar_t);
    return std::wstring(reinterpret_cast<const wchar_t*>(buffer.data()), chars);
}

// Reads RTL_USER_PROCESS_PARAMETERS and its embedded strings.
void fillProcessParameters(const ntapi::INativeApi& native, HANDLE process,
                           std::uint64_t address, ProcessParametersInfo& out) {
    out.address = address;

    ntapi::RtlUserProcessParametersPartial params{};
    if (!readRemote(native, process, address, params)) {
        return;
    }
    out.imagePathName = readRemoteUnicodeString(native, process, params.ImagePathName);
    out.commandLine = readRemoteUnicodeString(native, process, params.CommandLine);
    out.currentDirectory = readRemoteUnicodeString(native, process, params.CurrentDirPath);
}

// Reads PEB_LDR_DATA and records its list-head addresses.
void fillLoaderData(const ntapi::INativeApi& native, HANDLE process, std::uint64_t address,
                    LoaderDataInfo& out) {
    out.address = address;

    ntapi::PebLdrDataPartial ldr{};
    if (!readRemote(native, process, address, ldr)) {
        return;
    }
    out.initialized = ldr.Initialized != FALSE;

    // The list heads are embedded in the structure; their addresses are the
    // PEB_LDR_DATA base plus the field offsets.
    out.inLoadOrderListHead =
        address + offsetof(ntapi::PebLdrDataPartial, InLoadOrderModuleList);
    out.inMemoryOrderListHead =
        address + offsetof(ntapi::PebLdrDataPartial, InMemoryOrderModuleList);
    out.inInitOrderListHead =
        address + offsetof(ntapi::PebLdrDataPartial, InInitializationOrderModuleList);
}

}  // namespace

PebTebReader::PebTebReader(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<PebInfo, ErrorCode> PebTebReader::readPeb(std::uint32_t pid) const {
    auto processResult = native_.openProcess(pid, kProcessAccess);
    if (!processResult) {
        return makeUnexpected(std::move(processResult).error());
    }
    const Handle process = std::move(processResult).value();

    auto basicResult = native_.queryProcessBasicInformation(process.get());
    if (!basicResult) {
        return makeUnexpected(std::move(basicResult).error());
    }
    const auto pebAddress =
        reinterpret_cast<std::uint64_t>(basicResult.value().PebBaseAddress);
    if (pebAddress == 0) {
        return makeUnexpected(ErrorCode::application("Process has no accessible PEB"));
    }

    ntapi::PebPartial peb{};
    if (!readRemote(native_, process.get(), pebAddress, peb)) {
        return makeUnexpected(ErrorCode::application("Failed to read the PEB"));
    }

    PebInfo info;
    info.address = pebAddress;
    info.beingDebugged = peb.BeingDebugged != FALSE;
    info.imageBaseAddress = reinterpret_cast<std::uint64_t>(peb.ImageBaseAddress);
    info.ldrAddress = reinterpret_cast<std::uint64_t>(peb.Ldr);
    info.processParametersAddress = reinterpret_cast<std::uint64_t>(peb.ProcessParameters);
    info.processHeapAddress = reinterpret_cast<std::uint64_t>(peb.ProcessHeap);

    // Nested reads are best-effort: the PEB itself is the authoritative result.
    if (info.processParametersAddress != 0) {
        fillProcessParameters(native_, process.get(), info.processParametersAddress,
                              info.parameters);
    }
    if (info.ldrAddress != 0) {
        fillLoaderData(native_, process.get(), info.ldrAddress, info.loaderData);
    }

    return info;
}

Result<TebInfo, ErrorCode> PebTebReader::readTeb(std::uint32_t pid, std::uint32_t tid) const {
    // The TEB address comes from the thread; the memory read goes through the
    // owning process, since the TEB lives in that address space.
    auto threadResult = native_.openThread(tid, kThreadAccess);
    if (!threadResult) {
        return makeUnexpected(std::move(threadResult).error());
    }
    const Handle thread = std::move(threadResult).value();

    auto threadBasic = native_.queryThreadBasicInformation(thread.get());
    if (!threadBasic) {
        return makeUnexpected(std::move(threadBasic).error());
    }
    const auto tebAddress =
        reinterpret_cast<std::uint64_t>(threadBasic.value().TebBaseAddress);
    if (tebAddress == 0) {
        return makeUnexpected(ErrorCode::application("Thread has no accessible TEB"));
    }

    auto processResult = native_.openProcess(pid, kProcessAccess);
    if (!processResult) {
        return makeUnexpected(std::move(processResult).error());
    }
    const Handle process = std::move(processResult).value();

    ntapi::TebPartial teb{};
    if (!readRemote(native_, process.get(), tebAddress, teb)) {
        return makeUnexpected(ErrorCode::application("Failed to read the TEB"));
    }

    TebInfo info;
    info.address = tebAddress;
    info.stackBase = reinterpret_cast<std::uint64_t>(teb.StackBase);
    info.stackLimit = reinterpret_cast<std::uint64_t>(teb.StackLimit);
    info.selfPointer = reinterpret_cast<std::uint64_t>(teb.Self);
    info.pebAddress = reinterpret_cast<std::uint64_t>(teb.ProcessEnvironmentBlock);
    info.tlsPointer = reinterpret_cast<std::uint64_t>(teb.ThreadLocalStoragePointer);
    info.lastErrorValue = teb.LastErrorValue;
    info.ownerPid = pid;
    info.tid = tid;

    // TLS slots sit far inside the TEB; read the array directly by offset.
    const std::uint64_t slotsAddress = tebAddress + ntapi::teb_offsets::TlsSlotsX64;
    const std::size_t slotsBytes =
        ntapi::teb_offsets::TlsSlotCount * sizeof(std::uint64_t);

    std::vector<std::byte> slotBuffer(slotsBytes);
    auto slotsResult = native_.readMemory(process.get(), slotsAddress,
                                          MutableByteSpan(slotBuffer.data(), slotsBytes));
    if (slotsResult && slotsResult.value() == slotsBytes) {
        info.tlsSlots.resize(ntapi::teb_offsets::TlsSlotCount);
        std::memcpy(info.tlsSlots.data(), slotBuffer.data(), slotsBytes);
    }

    return info;
}

}  // namespace wis::core
