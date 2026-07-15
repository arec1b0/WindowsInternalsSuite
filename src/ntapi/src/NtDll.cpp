#include "ntapi/NtDll.hpp"

namespace wis::ntapi {
namespace {

// Resolves a single ntdll export to a typed function pointer, or nullptr.
template <typename Fn>
[[nodiscard]] Fn resolve(HMODULE module, const char* name) noexcept {
    // GetProcAddress returns FARPROC; route through void* before the typed cast
    // to keep the function-pointer conversion explicit and warning-clean.
    FARPROC proc = ::GetProcAddress(module, name);
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(proc));
}

}  // namespace

NtDll::NtDll() {
    // ntdll.dll is mapped into every process; GetModuleHandle cannot fail here.
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return;  // leave all pointers null; callers handle the missing binding
    }

    querySystemInformation_ =
        resolve<NtQuerySystemInformationFn>(ntdll, "NtQuerySystemInformation");
    queryInformationProcess_ =
        resolve<NtQueryInformationProcessFn>(ntdll, "NtQueryInformationProcess");
    queryInformationThread_ =
        resolve<NtQueryInformationThreadFn>(ntdll, "NtQueryInformationThread");
    readVirtualMemory_ = resolve<NtReadVirtualMemoryFn>(ntdll, "NtReadVirtualMemory");
    writeVirtualMemory_ = resolve<NtWriteVirtualMemoryFn>(ntdll, "NtWriteVirtualMemory");
    queryVirtualMemory_ = resolve<NtQueryVirtualMemoryFn>(ntdll, "NtQueryVirtualMemory");
    openProcess_ = resolve<NtOpenProcessFn>(ntdll, "NtOpenProcess");
    openThread_ = resolve<NtOpenThreadFn>(ntdll, "NtOpenThread");
    queryObject_ = resolve<NtQueryObjectFn>(ntdll, "NtQueryObject");
    duplicateObject_ = resolve<NtDuplicateObjectFn>(ntdll, "NtDuplicateObject");
    close_ = resolve<NtCloseFn>(ntdll, "NtClose");
    createQueryDebugBuffer_ =
        resolve<RtlCreateQueryDebugBufferFn>(ntdll, "RtlCreateQueryDebugBuffer");
    queryProcessDebugInformation_ =
        resolve<RtlQueryProcessDebugInformationFn>(ntdll, "RtlQueryProcessDebugInformation");
    destroyQueryDebugBuffer_ =
        resolve<RtlDestroyQueryDebugBufferFn>(ntdll, "RtlDestroyQueryDebugBuffer");
    getVersion_ = resolve<RtlGetVersionFn>(ntdll, "RtlGetVersion");
}

const NtDll& NtDll::instance() {
    // Thread-safe initialization guaranteed by the C++ standard (magic statics).
    static const NtDll dll;
    return dll;
}

}  // namespace wis::ntapi