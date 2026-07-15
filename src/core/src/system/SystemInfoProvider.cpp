#include "core/system/SystemInfoProvider.hpp"

#include <windows.h>

#include <intrin.h>

#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "common/text/Encoding.hpp"
#include "ntapi/NtTypes.hpp"

namespace wis::core {
namespace {

// Windows 11 shares major version 10 with Windows 10; the build number is the
// only reliable discriminator.
constexpr std::uint32_t kWindows11MinBuild = 22000;

// Reads the CPU brand string via CPUID leaves 0x80000002..0x80000004.
std::wstring queryCpuBrandString() {
    std::array<int, 4> registers{};

    // Leaf 0x80000000 reports the highest extended leaf supported.
    __cpuid(registers.data(), 0x80000000);
    const auto maxExtendedLeaf = static_cast<unsigned int>(registers[0]);
    if (maxExtendedLeaf < 0x80000004U) {
        return {};
    }

    char brand[49] = {};  // 3 leaves x 16 bytes + terminator
    for (unsigned int leaf = 0x80000002U; leaf <= 0x80000004U; ++leaf) {
        __cpuid(registers.data(), static_cast<int>(leaf));
        std::memcpy(brand + (leaf - 0x80000002U) * 16, registers.data(), 16);
    }

    // The brand string is often left-padded with spaces; trim them.
    std::string trimmed(brand);
    const std::size_t first = trimmed.find_first_not_of(' ');
    if (first == std::string::npos) {
        return {};
    }
    trimmed = trimmed.substr(first);
    while (!trimmed.empty() && trimmed.back() == ' ') {
        trimmed.pop_back();
    }
    return encoding::utf8ToWide(trimmed);
}

CacheType cacheTypeFromRaw(PROCESSOR_CACHE_TYPE type) noexcept {
    switch (type) {
        case CacheUnified:     return CacheType::Unified;
        case CacheInstruction: return CacheType::Instruction;
        case CacheData:        return CacheType::Data;
        case CacheTrace:       return CacheType::Trace;
        default:               return CacheType::Unknown;
    }
}

// Counts set bits in a processor affinity mask.
std::uint32_t popcount(KAFFINITY mask) noexcept {
    std::uint32_t count = 0;
    while (mask != 0) {
        mask &= (mask - 1);
        ++count;
    }
    return count;
}

// Walks GetLogicalProcessorInformationEx(RelationAll) to fill topology, caches,
// and NUMA nodes. Records are variable-length; the walk advances by ->Size.
void fillTopology(CpuInfo& cpu, std::vector<NumaNodeInfo>& numaNodes) {
    DWORD length = 0;
    ::GetLogicalProcessorInformationEx(RelationAll, nullptr, &length);
    if (length == 0 || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return;
    }

    std::vector<std::byte> buffer(length);
    if (::GetLogicalProcessorInformationEx(
            RelationAll,
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()),
            &length) == FALSE) {
        return;
    }

    std::size_t offset = 0;
    while (offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) <= buffer.size()) {
        const auto* record =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                buffer.data() + offset);
        if (record->Size == 0 || offset + record->Size > buffer.size()) {
            break;  // malformed or truncated; stop walking
        }

        switch (record->Relationship) {
            case RelationProcessorCore: {
                ++cpu.physicalCores;
                // A core contributes one logical processor per set affinity bit
                // (SMT/hyper-threading yields more than one).
                for (WORD g = 0; g < record->Processor.GroupCount; ++g) {
                    cpu.logicalProcessors += popcount(record->Processor.GroupMask[g].Mask);
                }
                break;
            }
            case RelationProcessorPackage: {
                ++cpu.processorPackages;
                break;
            }
            case RelationCache: {
                CacheInfo cache;
                cache.level = record->Cache.Level;
                cache.type = cacheTypeFromRaw(record->Cache.Type);
                cache.sizeBytes = record->Cache.CacheSize;
                cache.lineSizeBytes = record->Cache.LineSize;
                cache.associativity = record->Cache.Associativity;
                cpu.caches.push_back(cache);
                break;
            }
            case RelationNumaNode: {
                NumaNodeInfo node;
                node.nodeNumber = record->NumaNode.NodeNumber;
                node.processorMask =
                    static_cast<std::uint64_t>(record->NumaNode.GroupMask.Mask);
                node.processorGroup = record->NumaNode.GroupMask.Group;
                numaNodes.push_back(node);
                break;
            }
            case RelationGroup: {
                cpu.processorGroups = record->Group.ActiveGroupCount;
                break;
            }
            default:
                break;
        }

        offset += record->Size;
    }
}

void fillMemory(const ntapi::INativeApi& native, MemoryInfo& memory) {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) != FALSE) {
        memory.totalPhysicalBytes = status.ullTotalPhys;
        memory.availablePhysicalBytes = status.ullAvailPhys;
        memory.totalVirtualBytes = status.ullTotalVirtual;
        memory.availableVirtualBytes = status.ullAvailVirtual;
        memory.totalPageFileBytes = status.ullTotalPageFile;
        memory.availablePageFileBytes = status.ullAvailPageFile;
        memory.memoryLoadPercent = status.dwMemoryLoad;
    }

    // Page size and allocation granularity come from the native basic query.
    if (auto basic = native.querySystemBasicInformation()) {
        memory.pageSizeBytes = basic.value().PageSize;
        memory.allocationGranularity = basic.value().AllocationGranularity;
    }
}

// Derives a friendly product name from the version numbers.
std::wstring productNameFor(std::uint32_t major, std::uint32_t build, bool isServer) {
    if (major == 10) {
        if (isServer) {
            return L"Windows Server";
        }
        return (build >= kWindows11MinBuild) ? L"Windows 11" : L"Windows 10";
    }
    if (major == 6) {
        return isServer ? L"Windows Server (legacy)" : L"Windows (legacy)";
    }
    return L"Windows";
}

void fillOsVersion(const ntapi::INativeApi& native, OsVersionInfo& os) {
    auto versionResult = native.queryOsVersion();
    if (!versionResult) {
        return;
    }
    const RTL_OSVERSIONINFOEXW& raw = versionResult.value();

    os.majorVersion = raw.dwMajorVersion;
    os.minorVersion = raw.dwMinorVersion;
    os.buildNumber = raw.dwBuildNumber;
    os.isServer = raw.wProductType != VER_NT_WORKSTATION;
    os.servicePack = std::wstring(raw.szCSDVersion);
    os.productName = productNameFor(os.majorVersion, os.buildNumber, os.isServer);
}

void fillPagefiles(const ntapi::INativeApi& native, std::uint32_t pageSize,
                   std::vector<PagefileInfo>& pagefiles) {
    auto bufferResult = native.queryPagefileSnapshot();
    if (!bufferResult) {
        return;
    }
    const std::vector<std::byte>& buffer = bufferResult.value();
    if (buffer.empty()) {
        return;  // no pagefile configured
    }

    // Sizes are reported in pages; convert to bytes using the system page size.
    const std::uint64_t pageBytes = (pageSize != 0) ? pageSize : 4096;

    std::size_t offset = 0;
    while (offset + sizeof(ntapi::SystemPagefileInformation) <= buffer.size()) {
        const auto* record =
            reinterpret_cast<const ntapi::SystemPagefileInformation*>(buffer.data() + offset);

        PagefileInfo info;
        info.name = ntapi::unicodeStringToWide(record->PageFileName);
        info.totalBytes = static_cast<std::uint64_t>(record->TotalSize) * pageBytes;
        info.inUseBytes = static_cast<std::uint64_t>(record->TotalInUse) * pageBytes;
        info.peakUsageBytes = static_cast<std::uint64_t>(record->PeakUsage) * pageBytes;
        pagefiles.push_back(std::move(info));

        if (record->NextEntryOffset == 0) {
            break;  // last entry
        }
        const std::size_t next = offset + record->NextEntryOffset;
        if (next <= offset || next >= buffer.size()) {
            break;  // malformed chain
        }
        offset = next;
    }
}

}  // namespace

SystemInfoProvider::SystemInfoProvider(const ntapi::INativeApi& native) noexcept
    : native_(native) {}

Result<SystemInfo, ErrorCode> SystemInfoProvider::collect() const {
    SystemInfo info;

    info.cpu.brandString = queryCpuBrandString();
    fillTopology(info.cpu, info.numaNodes);
    fillMemory(native_, info.memory);
    fillOsVersion(native_, info.os);
    fillPagefiles(native_, info.memory.pageSizeBytes, info.pagefiles);

    // Milliseconds since boot; GetTickCount64 does not wrap like GetTickCount.
    info.uptimeMilliseconds = ::GetTickCount64();

    // A completely empty result means every sub-query failed.
    if (info.cpu.logicalProcessors == 0 && info.memory.totalPhysicalBytes == 0 &&
        info.os.majorVersion == 0) {
        return makeUnexpected(ErrorCode::application("System information unavailable"));
    }

    return info;
}

}  // namespace wis::core
