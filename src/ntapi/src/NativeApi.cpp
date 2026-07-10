#include "ntapi/INativeApi.hpp"

#include "ntapi/ObjectInformation.hpp"
#include "ntapi/ProcessInformation.hpp"
#include "ntapi/SystemInformation.hpp"
#include "ntapi/ThreadInformation.hpp"
#include "ntapi/VirtualMemory.hpp"

namespace wis::ntapi {
namespace {

// Production INativeApi: a thin dispatcher onto the free-function wrappers.
// Stateless, so a single instance is safe to share across threads.
class NativeApi final : public INativeApi {
public:
    Result<std::vector<std::byte>, ErrorCode> queryProcessSnapshot() const override {
        return system_info::queryProcesses();
    }

    Result<std::vector<std::byte>, ErrorCode> queryHandleSnapshot() const override {
        return system_info::queryHandles();
    }

    Result<SystemBasicInformation, ErrorCode> querySystemBasicInformation() const override {
        return system_info::queryBasic();
    }

    Result<Handle, ErrorCode> openProcess(std::uint32_t pid,
                                          ACCESS_MASK desiredAccess) const override {
        return process_info::open(pid, desiredAccess);
    }

    Result<ProcessBasicInformation, ErrorCode> queryProcessBasicInformation(
        HANDLE process) const override {
        return process_info::queryBasic(process);
    }

    Result<std::wstring, ErrorCode> queryProcessCommandLine(HANDLE process) const override {
        return process_info::queryCommandLine(process);
    }

    Result<std::wstring, ErrorCode> queryProcessImageName(HANDLE process) const override {
        return process_info::queryImageName(process);
    }

    Result<Handle, ErrorCode> openThread(std::uint32_t tid,
                                         ACCESS_MASK desiredAccess) const override {
        return thread_info::open(tid, desiredAccess);
    }

    Result<ThreadBasicInformation, ErrorCode> queryThreadBasicInformation(
        HANDLE thread) const override {
        return thread_info::queryBasic(thread);
    }

    Result<void*, ErrorCode> queryThreadStartAddress(HANDLE thread) const override {
        return thread_info::queryWin32StartAddress(thread);
    }

    Result<MEMORY_BASIC_INFORMATION, ErrorCode> queryMemoryRegion(
        HANDLE process, std::uint64_t address) const override {
        return memory::queryRegion(process, address);
    }

    Result<std::size_t, ErrorCode> readMemory(HANDLE process, std::uint64_t address,
                                              MutableByteSpan buffer) const override {
        return memory::read(process, address, buffer);
    }

    Result<std::size_t, ErrorCode> writeMemory(HANDLE process, std::uint64_t address,
                                               ByteSpan buffer) const override {
        return memory::write(process, address, buffer);
    }

    Result<std::wstring, ErrorCode> queryMappedFilename(
        HANDLE process, std::uint64_t address) const override {
        return memory::queryMappedFilename(process, address);
    }

    Result<std::wstring, ErrorCode> queryObjectName(HANDLE handle) const override {
        return object_info::queryName(handle);
    }

    Result<std::wstring, ErrorCode> queryObjectType(HANDLE handle) const override {
        return object_info::queryType(handle);
    }

    Result<Handle, ErrorCode> duplicateHandle(HANDLE sourceProcess,
                                              HANDLE sourceHandle) const override {
        return object_info::duplicateIntoSelf(sourceProcess, sourceHandle);
    }
};

}  // namespace

std::unique_ptr<INativeApi> createNativeApi() {
    return std::make_unique<NativeApi>();
}

}  // namespace wis::ntapi