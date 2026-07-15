#pragma once

// Domain models for system-wide information: CPU topology, caches, NUMA nodes,
// physical memory, OS version, uptime, and pagefile usage.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wis::core {

enum class CacheType : std::uint8_t {
    Unified,
    Instruction,
    Data,
    Trace,
    Unknown,
};

[[nodiscard]] constexpr std::string_view toString(CacheType type) noexcept {
    switch (type) {
        case CacheType::Unified:     return "Unified";
        case CacheType::Instruction: return "Instruction";
        case CacheType::Data:        return "Data";
        case CacheType::Trace:       return "Trace";
        case CacheType::Unknown:
            break;
    }
    return "Unknown";
}

struct CacheInfo {
    std::uint8_t level = 0;         // 1, 2, 3
    CacheType type = CacheType::Unknown;
    std::uint32_t sizeBytes = 0;
    std::uint16_t lineSizeBytes = 0;
    std::uint8_t associativity = 0; // 0xFF means fully associative
};

struct NumaNodeInfo {
    std::uint32_t nodeNumber = 0;
    std::uint64_t processorMask = 0;
    std::uint16_t processorGroup = 0;
};

struct CpuInfo {
    std::wstring brandString;       // e.g. "12th Gen Intel(R) Core(TM) i7-12700H"
    std::uint32_t physicalCores = 0;
    std::uint32_t logicalProcessors = 0;
    std::uint32_t processorPackages = 0;
    std::uint32_t processorGroups = 0;
    std::vector<CacheInfo> caches;
};

struct MemoryInfo {
    std::uint64_t totalPhysicalBytes = 0;
    std::uint64_t availablePhysicalBytes = 0;
    std::uint64_t totalVirtualBytes = 0;
    std::uint64_t availableVirtualBytes = 0;
    std::uint64_t totalPageFileBytes = 0;   // commit limit
    std::uint64_t availablePageFileBytes = 0;
    std::uint32_t memoryLoadPercent = 0;
    std::uint32_t pageSizeBytes = 0;
    std::uint32_t allocationGranularity = 0;
};

struct OsVersionInfo {
    std::uint32_t majorVersion = 0;
    std::uint32_t minorVersion = 0;
    std::uint32_t buildNumber = 0;
    std::wstring servicePack;
    std::wstring productName;       // e.g. "Windows 11" (derived from build)
    bool isServer = false;
};

struct PagefileInfo {
    std::wstring name;              // native device path
    std::uint64_t totalBytes = 0;
    std::uint64_t inUseBytes = 0;
    std::uint64_t peakUsageBytes = 0;
};

struct SystemInfo {
    CpuInfo cpu;
    MemoryInfo memory;
    OsVersionInfo os;
    std::vector<NumaNodeInfo> numaNodes;
    std::vector<PagefileInfo> pagefiles;
    std::uint64_t uptimeMilliseconds = 0;
};

}  // namespace wis::core
