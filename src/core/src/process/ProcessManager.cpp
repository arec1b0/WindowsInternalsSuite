#include "core/process/ProcessManager.hpp"

#include <windows.h>

#include <utility>

#include "core/detail/SnapshotWalk.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// Access mask sufficient for path/command-line/architecture queries on modern
// Windows without requiring full PROCESS_ALL_ACCESS.
constexpr ACCESS_MASK kDetailAccess = PROCESS_QUERY_LIMITED_INFORMATION;

std::uint32_t handleToPid(HANDLE value) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<ULONG_PTR>(value));
}

Architecture architectureFromMachine(USHORT machine) noexcept {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:  return Architecture::X86;
        case IMAGE_FILE_MACHINE_AMD64: return Architecture::X64;
        case IMAGE_FILE_MACHINE_ARM64: return Architecture::Arm64;
        default:                       return Architecture::Unknown;
    }
}

// Maps one native process record onto the domain model (cheap fields only).
ProcessInfo toProcessInfo(const ntapi::SystemProcessInformation& record) {
    ProcessInfo info;
    info.pid = handleToPid(record.UniqueProcessId);
    info.parentPid = handleToPid(record.InheritedFromUniqueProcessId);
    info.imageName = ntapi::unicodeStringToWide(record.ImageName);
    info.sessionId = record.SessionId;
    info.threadCount = record.NumberOfThreads;
    info.handleCount = record.HandleCount;
    info.basePriority = record.BasePriority;

    info.createTime = static_cast<std::uint64_t>(record.CreateTime.QuadPart);
    info.kernelTime = static_cast<std::uint64_t>(record.KernelTime.QuadPart);
    info.userTime = static_cast<std::uint64_t>(record.UserTime.QuadPart);
    info.cycleTime = record.CycleTime;

    info.workingSetBytes = record.WorkingSetSize;
    info.privateBytes = record.PrivatePageCount;
    info.virtualSizeBytes = record.VirtualSize;

    // The idle process has an empty image name; give it a conventional label.
    if (info.pid == 0 && info.imageName.empty()) {
        info.imageName = L"System Idle Process";
    }
    return info;
}

// Resolves the full DOS image path, growing the buffer for long paths.
std::wstring queryFullPath(HANDLE process) {
    std::wstring path(1024, L'\0');
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto size = static_cast<DWORD>(path.size());
        if (::QueryFullProcessImageNameW(process, 0, path.data(), &size) != FALSE) {
            path.resize(size);
            return path;
        }
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }
        path.resize(path.size() * 2);
    }
    return {};
}

// Determines process architecture via IsWow64Process2 (Win10 1709+).
Architecture queryArchitecture(HANDLE process) {
    USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    if (::IsWow64Process2(process, &processMachine, &nativeMachine) == FALSE) {
        return Architecture::Unknown;
    }
    // A native (non-WOW64) process reports UNKNOWN for the guest machine; its
    // architecture is then the host's native machine.
    if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN) {
        return architectureFromMachine(nativeMachine);
    }
    return architectureFromMachine(processMachine);
}

}  // namespace

ProcessManager::ProcessManager(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<ProcessInfo>, ErrorCode> ProcessManager::snapshot() const {
    auto bufferResult = native_.queryProcessSnapshot();
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    std::vector<ProcessInfo> processes;
    auto walkResult = detail::forEachProcess(
        bufferResult.value(),
        [&](const ntapi::SystemProcessInformation& record,
            std::span<const ntapi::SystemThreadInformation>) {
            processes.push_back(toProcessInfo(record));
            return true;  // visit every process
        });
    if (!walkResult) {
        return makeUnexpected(std::move(walkResult).error());
    }

    return processes;
}

Result<ProcessDetails, ErrorCode> ProcessManager::queryDetails(std::uint32_t pid) const {
    auto handleResult = native_.openProcess(pid, kDetailAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle process = std::move(handleResult).value();

    ProcessDetails details;
    details.fullImagePath = queryFullPath(process.get());
    details.architecture = queryArchitecture(process.get());

    // Command line is optional; a failure here does not invalidate the rest.
    auto commandLineResult = native_.queryProcessCommandLine(process.get());
    if (commandLineResult) {
        details.commandLine = std::move(commandLineResult).value();
    }

    return details;
}

}  // namespace wis::core