#include "ntapi/INativeApi.hpp"

#include "ntapi/ObjectInformation.hpp"
#include "ntapi/ProcessInformation.hpp"
#include "ntapi/SystemInformation.hpp"
#include "ntapi/ThreadInformation.hpp"
#include "ntapi/VirtualMemory.hpp"

#include <psapi.h>

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

    Result<std::vector<std::uint64_t>, ErrorCode> enumProcessModules(
        HANDLE process) const override {
        // Two-pass: first call sizes the array, second fills it. LIST_MODULES_ALL
        // returns both 32- and 64-bit modules where applicable.
        DWORD needed = 0;
        if (::EnumProcessModulesEx(process, nullptr, 0, &needed, LIST_MODULES_ALL) == FALSE) {
            return makeUnexpected(ErrorCode::fromLastError());
        }
        if (needed == 0) {
            return std::vector<std::uint64_t>{};
        }

        std::vector<HMODULE> handles(needed / sizeof(HMODULE));
        DWORD returned = 0;
        if (::EnumProcessModulesEx(process, handles.data(),
                                   static_cast<DWORD>(handles.size() * sizeof(HMODULE)),
                                   &returned, LIST_MODULES_ALL) == FALSE) {
            return makeUnexpected(ErrorCode::fromLastError());
        }

        const std::size_t count = returned / sizeof(HMODULE);
        std::vector<std::uint64_t> bases;
        bases.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            bases.push_back(reinterpret_cast<std::uint64_t>(handles[i]));
        }
        return bases;
    }

    Result<std::wstring, ErrorCode> queryModuleFileName(
        HANDLE process, std::uint64_t moduleBase) const override {
        std::wstring path(1024, L'\0');
        for (int attempt = 0; attempt < 4; ++attempt) {
            const DWORD length = ::GetModuleFileNameExW(
                process, reinterpret_cast<HMODULE>(moduleBase), path.data(),
                static_cast<DWORD>(path.size()));
            if (length == 0) {
                return makeUnexpected(ErrorCode::fromLastError());
            }
            // Truncation is signaled by length == buffer size; grow and retry.
            if (length < path.size()) {
                path.resize(length);
                return path;
            }
            path.resize(path.size() * 2);
        }
        return makeUnexpected(ErrorCode::application("Module path exceeds buffer growth"));
    }

    Result<ModuleRawInfo, ErrorCode> queryModuleInformation(
        HANDLE process, std::uint64_t moduleBase) const override {
        MODULEINFO mi{};
        if (::GetModuleInformation(process, reinterpret_cast<HMODULE>(moduleBase), &mi,
                                   sizeof(mi)) == FALSE) {
            return makeUnexpected(ErrorCode::fromLastError());
        }
        ModuleRawInfo info;
        info.baseAddress = reinterpret_cast<std::uint64_t>(mi.lpBaseOfDll);
        info.entryPoint = reinterpret_cast<std::uint64_t>(mi.EntryPoint);
        info.sizeOfImage = mi.SizeOfImage;
        return info;
    }
};

}  // namespace

std::unique_ptr<INativeApi> createNativeApi() {
    return std::make_unique<NativeApi>();
}

}  // namespace wis::ntapi