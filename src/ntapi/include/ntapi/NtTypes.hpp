#pragma once

// Native API ABI structures, defined locally to avoid <winternl.h> conflicts.
// Layout must match the OS binary interface exactly. Targets Windows 10/11 x64.
//
// Field *presence* is stable across Win10/11 x64; individual field *validity*
// may vary by build. static_assert guards catch size drift at compile time.

#include <windows.h>

#include <cstddef>
#include <string>

#include "ntapi/NtStatus.hpp"

namespace wis::ntapi {

// -----------------------------------------------------------------------------
// Fundamental native types
// -----------------------------------------------------------------------------

// Counted UTF-16 string. Buffer is NOT guaranteed null-terminated; use Length.
struct UnicodeString {
    USHORT Length;         // bytes, excluding terminator
    USHORT MaximumLength;  // bytes, capacity of Buffer
    PWSTR Buffer;
};
static_assert(sizeof(UnicodeString) == 16, "UnicodeString ABI layout (x64)");

struct ClientId {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
};
static_assert(sizeof(ClientId) == 16, "ClientId ABI layout (x64)");

struct ObjectAttributes {
    ULONG Length;
    HANDLE RootDirectory;
    UnicodeString* ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
};
static_assert(sizeof(ObjectAttributes) == 48, "ObjectAttributes ABI layout (x64)");

// -----------------------------------------------------------------------------
// Information-class enumerations (subsets actually queried by this suite)
// -----------------------------------------------------------------------------

enum class SystemInformationClass : ULONG {
    Basic            = 0,   // SYSTEM_BASIC_INFORMATION
    Process          = 5,   // SYSTEM_PROCESS_INFORMATION (+ thread array)
    Handle           = 16,  // SYSTEM_HANDLE_INFORMATION (legacy)
    ExtendedHandle   = 64,  // SYSTEM_HANDLE_INFORMATION_EX
};

enum class ProcessInformationClass : ULONG {
    Basic            = 0,   // PROCESS_BASIC_INFORMATION
    ImageFileName    = 27,  // UNICODE_STRING (native device path)
    CommandLine      = 60,  // PROCESS_COMMAND_LINE_INFORMATION (Win8.1+)
};

enum class ThreadInformationClass : ULONG {
    Basic            = 0,   // THREAD_BASIC_INFORMATION
    Win32StartAddress = 9,  // PVOID
};

enum class ObjectInformationClass : ULONG {
    Basic            = 0,   // OBJECT_BASIC_INFORMATION
    Name             = 1,   // OBJECT_NAME_INFORMATION
    Type             = 2,   // OBJECT_TYPE_INFORMATION
    Types            = 3,   // OBJECT_TYPES_INFORMATION
};

enum class MemoryInformationClass : ULONG {
    Basic            = 0,   // MEMORY_BASIC_INFORMATION (in winnt.h)
    MappedFilename   = 2,   // UNICODE_STRING (backing file, native path)
};

// -----------------------------------------------------------------------------
// System information payloads
// -----------------------------------------------------------------------------

struct SystemBasicInformation {
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    ULONG_PTR ActiveProcessorsAffinityMask;
    CCHAR NumberOfProcessors;
};

// Per-thread record embedded in the SYSTEM_PROCESS_INFORMATION thread array.
struct SystemThreadInformation {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    ClientId ClientId;
    LONG Priority;
    LONG BasePriority;
    ULONG ContextSwitches;
    ULONG ThreadState;   // KTHREAD_STATE
    ULONG WaitReason;    // KWAIT_REASON
};

// Variable-length record. Walk the chain via NextEntryOffset until it is 0.
// The thread array (SystemThreadInformation[NumberOfThreads]) follows inline.
struct SystemProcessInformation {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UnicodeString ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
};

// -----------------------------------------------------------------------------
// Handle enumeration (SystemExtendedHandleInformation)
// -----------------------------------------------------------------------------

struct SystemHandleTableEntryInfoEx {
    PVOID Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
};
static_assert(sizeof(SystemHandleTableEntryInfoEx) == 40,
              "SystemHandleTableEntryInfoEx ABI layout (x64)");

// Variable-length: Handles[NumberOfHandles] follows the header inline.
struct SystemHandleInformationEx {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SystemHandleTableEntryInfoEx Handles[1];
};

// -----------------------------------------------------------------------------
// Process / thread information payloads
// -----------------------------------------------------------------------------

struct ProcessBasicInformation {
    NtStatus ExitStatus;
    PVOID PebBaseAddress;          // address of the PEB in the target process
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
};
static_assert(sizeof(ProcessBasicInformation) == 48,
              "ProcessBasicInformation ABI layout (x64)");

struct ThreadBasicInformation {
    NtStatus ExitStatus;
    PVOID TebBaseAddress;          // address of the TEB in the target process
    ClientId ClientId;
    ULONG_PTR AffinityMask;
    LONG Priority;
    LONG BasePriority;
};
static_assert(sizeof(ThreadBasicInformation) == 48,
              "ThreadBasicInformation ABI layout (x64)");

// -----------------------------------------------------------------------------
// Object information payloads
// -----------------------------------------------------------------------------

struct ObjectBasicInformation {
    ULONG Attributes;
    ACCESS_MASK GrantedAccess;
    ULONG HandleCount;
    ULONG PointerCount;
    ULONG PagedPoolCharge;
    ULONG NonPagedPoolCharge;
    ULONG Reserved[3];
    ULONG NameInfoSize;
    ULONG TypeInfoSize;
    ULONG SecurityDescriptorSize;
    LARGE_INTEGER CreationTime;
};

// TypeName is the first field; the remaining statistics are rarely needed but
// kept for correct sizing when the object type table is walked.
struct ObjectTypeInformation {
    UnicodeString TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccessMask;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    UCHAR TypeIndex;
    CHAR ReservedByte;
    ULONG PoolType;
    ULONG DefaultPagedPoolCharge;
    ULONG DefaultNonPagedPoolCharge;
};

// Returned by NtQueryObject(ObjectNameInformation): a UNICODE_STRING whose
// Buffer points just past the struct within the same allocation.
struct ObjectNameInformation {
    UnicodeString Name;
};

// Returned by NtQueryInformationProcess(ProcessCommandLineInformation).
struct ProcessCommandLineInformation {
    UnicodeString CommandLine;
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Copies a native counted UTF-16 string into a std::wstring. The Buffer is not
// assumed null-terminated; Length (in bytes) is authoritative.
[[nodiscard]] inline std::wstring unicodeStringToWide(const UnicodeString& value) {
    if (value.Buffer == nullptr || value.Length == 0) {
        return {};
    }
    return std::wstring(value.Buffer, value.Length / sizeof(wchar_t));
}

// -----------------------------------------------------------------------------
// Debug-buffer heap information (RtlQueryProcessDebugInformation)
// -----------------------------------------------------------------------------

// Query flags for RtlQueryProcessDebugInformation.
namespace debug_query {
constexpr ULONG Modules       = 0x00000001UL;  // RTL_QUERY_PROCESS_MODULES
constexpr ULONG HeapSummary   = 0x00000004UL;  // RTL_QUERY_PROCESS_HEAP_SUMMARY
constexpr ULONG Heaps         = 0x00000008UL;  // RTL_QUERY_PROCESS_HEAPS
constexpr ULONG HeapEntries   = 0x00000010UL;  // RTL_QUERY_PROCESS_HEAP_ENTRIES
}  // namespace debug_query

// Per-heap summary record within RTL_PROCESS_HEAPS.
struct RtlHeapInformation {
    PVOID BaseAddress;
    ULONG Flags;
    USHORT EntryOverhead;
    USHORT CreatorBackTraceIndex;
    SIZE_T BytesAllocated;
    SIZE_T BytesCommitted;
    ULONG NumberOfTags;
    ULONG NumberOfEntries;      // allocated block count
    ULONG NumberOfPseudoTags;
    ULONG PseudoTagGranularity;
    ULONG Reserved[5];
    PVOID Tags;
    PVOID Entries;              // valid only when HeapEntries was requested
};

// Variable-length: Heaps[NumberOfHeaps] follows the header inline.
struct RtlProcessHeaps {
    ULONG NumberOfHeaps;
    RtlHeapInformation Heaps[1];
};

// Opaque debug buffer header. Only the Heap/Module info pointers are needed; the
// rest of the (large) structure is reserved so sizeof is not relied upon — the
// buffer is always allocated by RtlCreateQueryDebugBuffer, never by us.
struct RtlDebugInformation {
    HANDLE SectionHandleClient;
    PVOID ViewBaseClient;
    PVOID ViewBaseTarget;
    ULONG_PTR ViewBaseDelta;
    HANDLE EventPairClient;
    HANDLE EventPairTarget;
    HANDLE TargetProcessId;
    HANDLE TargetThreadHandle;
    ULONG Flags;
    SIZE_T OffsetFree;
    SIZE_T CommitSize;
    SIZE_T ViewSize;
    PVOID Modules;                 // RTL_PROCESS_MODULES* (when Modules queried)
    PVOID BackTraces;
    RtlProcessHeaps* Heaps;        // RTL_PROCESS_HEAPS* (when Heaps queried)
    PVOID Locks;
    PVOID SpecificHeap;
    HANDLE TargetProcessHandle;
    PVOID Reserved[6];
};

// -----------------------------------------------------------------------------
// PEB / TEB (partial layouts, x64)
// -----------------------------------------------------------------------------
//
// These are deliberately PARTIAL: only the fields this suite reads are declared,
// with explicit padding to hold the offsets. Declaring the full PEB/TEB would
// bind the build to a specific Windows build's internal layout for no benefit.
// The offsets below are stable across Windows 10/11 x64.

#pragma pack(push, 1)

// PEB fields up to ProcessHeap (offset 0x38).
struct PebPartial {
    BOOLEAN InheritedAddressSpace;      // 0x00
    BOOLEAN ReadImageFileExecOptions;   // 0x01
    BOOLEAN BeingDebugged;              // 0x02
    BOOLEAN BitField;                   // 0x03
    UCHAR Padding0[4];                  // 0x04
    PVOID Mutant;                       // 0x08
    PVOID ImageBaseAddress;             // 0x10
    PVOID Ldr;                          // 0x18  PEB_LDR_DATA*
    PVOID ProcessParameters;            // 0x20  RTL_USER_PROCESS_PARAMETERS*
    PVOID SubSystemData;                // 0x28
    PVOID ProcessHeap;                  // 0x30
    PVOID FastPebLock;                  // 0x38
};
static_assert(offsetof(PebPartial, BeingDebugged) == 0x02, "PEB.BeingDebugged offset");
static_assert(offsetof(PebPartial, ImageBaseAddress) == 0x10, "PEB.ImageBaseAddress offset");
static_assert(offsetof(PebPartial, Ldr) == 0x18, "PEB.Ldr offset");
static_assert(offsetof(PebPartial, ProcessParameters) == 0x20, "PEB.ProcessParameters offset");
static_assert(offsetof(PebPartial, ProcessHeap) == 0x30, "PEB.ProcessHeap offset");

// PEB_LDR_DATA fields up to the module list heads.
struct PebLdrDataPartial {
    ULONG Length;                              // 0x00
    BOOLEAN Initialized;                       // 0x04
    UCHAR Padding0[3];                         // 0x05
    PVOID SsHandle;                            // 0x08
    LIST_ENTRY InLoadOrderModuleList;          // 0x10
    LIST_ENTRY InMemoryOrderModuleList;        // 0x20
    LIST_ENTRY InInitializationOrderModuleList;// 0x30
};
static_assert(offsetof(PebLdrDataPartial, InLoadOrderModuleList) == 0x10,
              "PEB_LDR_DATA.InLoadOrderModuleList offset");

// RTL_USER_PROCESS_PARAMETERS fields up to CommandLine (offset 0x70).
struct RtlUserProcessParametersPartial {
    ULONG MaximumLength;          // 0x00
    ULONG Length;                 // 0x04
    ULONG Flags;                  // 0x08
    ULONG DebugFlags;             // 0x0C
    PVOID ConsoleHandle;          // 0x10
    ULONG ConsoleFlags;           // 0x18
    UCHAR Padding0[4];            // 0x1C
    PVOID StandardInput;          // 0x20
    PVOID StandardOutput;         // 0x28
    PVOID StandardError;          // 0x30
    UnicodeString CurrentDirPath; // 0x38  (CURDIR.DosPath)
    PVOID CurrentDirHandle;       // 0x48  (CURDIR.Handle)
    UnicodeString DllPath;        // 0x50
    UnicodeString ImagePathName;  // 0x60
    UnicodeString CommandLine;    // 0x70
};
static_assert(offsetof(RtlUserProcessParametersPartial, CurrentDirPath) == 0x38,
              "RTL_USER_PROCESS_PARAMETERS.CurrentDirectory offset");
static_assert(offsetof(RtlUserProcessParametersPartial, ImagePathName) == 0x60,
              "RTL_USER_PROCESS_PARAMETERS.ImagePathName offset");
static_assert(offsetof(RtlUserProcessParametersPartial, CommandLine) == 0x70,
              "RTL_USER_PROCESS_PARAMETERS.CommandLine offset");

// TEB fields up to LastErrorValue (offset 0x68). NT_TIB occupies 0x00..0x37.
struct TebPartial {
    PVOID ExceptionList;              // 0x00  NtTib.ExceptionList
    PVOID StackBase;                  // 0x08  NtTib.StackBase
    PVOID StackLimit;                 // 0x10  NtTib.StackLimit
    PVOID SubSystemTib;               // 0x18
    PVOID FiberData;                  // 0x20
    PVOID ArbitraryUserPointer;       // 0x28
    PVOID Self;                       // 0x30  NtTib.Self
    PVOID EnvironmentPointer;         // 0x38
    ClientId TebClientId;             // 0x40
    PVOID ActiveRpcHandle;            // 0x50
    PVOID ThreadLocalStoragePointer;  // 0x58
    PVOID ProcessEnvironmentBlock;    // 0x60  -> PEB
    ULONG LastErrorValue;             // 0x68
};
static_assert(offsetof(TebPartial, StackBase) == 0x08, "TEB.NtTib.StackBase offset");
static_assert(offsetof(TebPartial, StackLimit) == 0x10, "TEB.NtTib.StackLimit offset");
static_assert(offsetof(TebPartial, ThreadLocalStoragePointer) == 0x58,
              "TEB.ThreadLocalStoragePointer offset");
static_assert(offsetof(TebPartial, ProcessEnvironmentBlock) == 0x60, "TEB.PEB offset");
static_assert(offsetof(TebPartial, LastErrorValue) == 0x68, "TEB.LastErrorValue offset");

#pragma pack(pop)

// TLS slot array lives deep inside the TEB; read it by offset rather than
// declaring the ~6 KB of intervening fields.
namespace teb_offsets {
constexpr std::uint32_t TlsSlotsX64 = 0x1480;  // PVOID TlsSlots[64]
constexpr std::uint32_t TlsSlotCount = 64;     // TLS_MINIMUM_AVAILABLE
}  // namespace teb_offsets

}  // namespace wis::ntapi