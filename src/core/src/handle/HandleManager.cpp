#include "core/handle/HandleManager.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "core/handle/ObjectTypeTable.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// Access to open the owning process for handle duplication.
constexpr ACCESS_MASK kDupAccess = PROCESS_DUP_HANDLE;

std::uint32_t handleToPid(std::uint64_t value) noexcept {
    return static_cast<std::uint32_t>(value);
}

}  // namespace

HandleManager::HandleManager(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<HandleInfo>, ErrorCode> HandleManager::snapshot(std::uint32_t pid) const {
    auto snapshotResult = native_.queryHandleSnapshot();
    if (!snapshotResult) {
        return makeUnexpected(std::move(snapshotResult).error());
    }
    const std::vector<std::byte>& buffer = snapshotResult.value();
    if (buffer.size() < sizeof(ntapi::SystemHandleInformationEx)) {
        return makeUnexpected(ErrorCode::application("Handle snapshot buffer too small"));
    }

    // Build the type table once; if it fails, type names are left blank rather
    // than failing the whole listing.
    std::optional<ObjectTypeTable> typeTable;
    if (auto built = ObjectTypeTable::build(native_)) {
        typeTable.emplace(std::move(built).value());
    }

    const std::byte* base = buffer.data();
    const auto* header = reinterpret_cast<const ntapi::SystemHandleInformationEx*>(base);
    const std::size_t handleCount = header->NumberOfHandles;

    // Validate that the declared count fits within the returned buffer.
    const std::size_t entriesOffset =
        offsetof(ntapi::SystemHandleInformationEx, Handles);
    const std::size_t needed =
        entriesOffset + handleCount * sizeof(ntapi::SystemHandleTableEntryInfoEx);
    if (needed > buffer.size()) {
        return makeUnexpected(ErrorCode::application("Handle snapshot count exceeds buffer"));
    }

    const auto* entries = reinterpret_cast<const ntapi::SystemHandleTableEntryInfoEx*>(
        base + entriesOffset);

    std::vector<HandleInfo> handles;
    for (std::size_t i = 0; i < handleCount; ++i) {
        const ntapi::SystemHandleTableEntryInfoEx& entry = entries[i];
        if (handleToPid(entry.UniqueProcessId) != pid) {
            continue;  // filter to the requested process
        }

        HandleInfo info;
        info.ownerPid = pid;
        info.handleValue = entry.HandleValue;
        info.objectAddress = reinterpret_cast<std::uint64_t>(entry.Object);
        info.typeIndex = entry.ObjectTypeIndex;
        info.grantedAccess = entry.GrantedAccess;
        info.attributes = entry.HandleAttributes;
        if (typeTable) {
            info.typeName = typeTable->nameFor(entry.ObjectTypeIndex);
        }
        handles.push_back(std::move(info));
    }

    return handles;
}

HandleObjectName HandleManager::resolveName(std::uint32_t ownerPid,
                                            std::uint64_t handleValue) const {
    HandleObjectName result;

    auto processResult = native_.openProcess(ownerPid, kDupAccess);
    if (!processResult) {
        return result;  // cannot open owner; leave unresolved
    }
    const Handle owner = std::move(processResult).value();

    auto dupResult = native_.duplicateHandle(
        owner.get(), reinterpret_cast<HANDLE>(handleValue));
    if (!dupResult) {
        return result;
    }
    Handle duplicate = std::move(dupResult).value();
    const HANDLE rawDuplicate = duplicate.get();

    // NtQueryObject(Name) can block indefinitely on some synchronous objects.
    // Run it on a detached thread and abandon it if it overruns; a detached
    // thread does not block on destruction the way std::async's future does.
    struct SharedState {
        std::mutex mutex;
        std::wstring name;
        std::atomic<bool> done{false};
    };
    auto state = std::make_shared<SharedState>();

    std::thread worker([this, rawDuplicate, state] {
        auto nameResult = native_.queryObjectName(rawDuplicate);
        {
            std::scoped_lock lock(state->mutex);
            if (nameResult) {
                state->name = std::move(nameResult).value();
            }
        }
        state->done.store(true, std::memory_order_release);
    });
    worker.detach();

    // Poll for completion up to the timeout.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kNameQueryTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (state->done.load(std::memory_order_acquire)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (!state->done.load(std::memory_order_acquire)) {
        result.timedOut = true;
        // The detached worker keeps `state` and the duplicated handle alive via
        // the shared_ptr capture; both are released if/when it eventually
        // unblocks. The UI is never held hostage.
        (void)duplicate.release();  // ownership transferred to the worker's lifetime
        return result;
    }

    std::scoped_lock lock(state->mutex);
    result.resolved = true;
    result.name = state->name;
    return result;
}

}  // namespace wis::core
