#include "core/thread/ThreadManager.hpp"

#include <windows.h>

#include <utility>

#include "core/detail/SnapshotWalk.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// THREAD_QUERY_LIMITED_INFORMATION is enough for Win32StartAddress and the TEB
// base on modern Windows, and succeeds against more processes than full query
// access.
constexpr ACCESS_MASK kDetailAccess = THREAD_QUERY_LIMITED_INFORMATION;

std::uint32_t handleToId(HANDLE value) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<ULONG_PTR>(value));
}

// Maps one native thread record onto the domain model.
ThreadInfo toThreadInfo(const ntapi::SystemThreadInformation& record) {
    ThreadInfo info;
    info.tid = handleToId(record.ClientId.UniqueThread);
    info.ownerPid = handleToId(record.ClientId.UniqueProcess);
    info.state = threadStateFromRaw(record.ThreadState);
    info.waitReason = record.WaitReason;
    info.priority = record.Priority;
    info.basePriority = record.BasePriority;
    info.startAddress = reinterpret_cast<std::uint64_t>(record.StartAddress);

    info.createTime = static_cast<std::uint64_t>(record.CreateTime.QuadPart);
    info.kernelTime = static_cast<std::uint64_t>(record.KernelTime.QuadPart);
    info.userTime = static_cast<std::uint64_t>(record.UserTime.QuadPart);
    info.contextSwitches = record.ContextSwitches;
    return info;
}

}  // namespace

ThreadManager::ThreadManager(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<ThreadInfo>, ErrorCode> ThreadManager::snapshot(std::uint32_t pid) const {
    auto bufferResult = native_.queryProcessSnapshot();
    if (!bufferResult) {
        return makeUnexpected(std::move(bufferResult).error());
    }

    std::vector<ThreadInfo> threads;
    bool found = false;

    auto walkResult = detail::forEachProcess(
        bufferResult.value(),
        [&](const ntapi::SystemProcessInformation& record,
            std::span<const ntapi::SystemThreadInformation> threadSpan) {
            if (handleToId(record.UniqueProcessId) != pid) {
                return true;  // keep scanning for the target process
            }
            found = true;
            threads.reserve(threadSpan.size());
            for (const auto& thread : threadSpan) {
                threads.push_back(toThreadInfo(thread));
            }
            return false;  // target found; stop walking the chain
        });
    if (!walkResult) {
        return makeUnexpected(std::move(walkResult).error());
    }

    // `found == false` means the PID was absent from the snapshot (exited or
    // never existed). An empty thread list is the correct, non-error result.
    (void)found;
    return threads;
}

Result<ThreadDetails, ErrorCode> ThreadManager::queryDetails(std::uint32_t tid) const {
    auto handleResult = native_.openThread(tid, kDetailAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle thread = std::move(handleResult).value();

    ThreadDetails details;

    // Win32 start address is best-effort; absence is not fatal to the query.
    auto startResult = native_.queryThreadStartAddress(thread.get());
    if (startResult) {
        details.win32StartAddress = reinterpret_cast<std::uint64_t>(startResult.value());
    }

    auto basicResult = native_.queryThreadBasicInformation(thread.get());
    if (basicResult) {
        details.tebBaseAddress =
            reinterpret_cast<std::uint64_t>(basicResult.value().TebBaseAddress);
    }

    return details;
}

}  // namespace wis::core
