#include "core/memory/MemoryManager.hpp"

#include <windows.h>

#include <utility>

#include "core/memory/SignatureScanner.hpp"
#include "ntapi/NtStatus.hpp"

namespace wis::core {
namespace {

// Query + read a foreign process address space.
constexpr ACCESS_MASK kReadAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

// Fallback upper bound for the user-mode address space on x64, used only when
// the system basic information query fails.
constexpr std::uint64_t kDefaultMaxUserAddress = 0x00007FFFFFFEFFFFULL;

// Chunk size for scanning; regions larger than this are read in windows that
// overlap by (patternSize - 1) so matches spanning a boundary are not missed.
constexpr std::size_t kScanChunk = 1U << 20;  // 1 MiB

MemoryRegion toMemoryRegion(const MEMORY_BASIC_INFORMATION& mbi) {
    MemoryRegion region;
    region.baseAddress = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
    region.allocationBase = reinterpret_cast<std::uint64_t>(mbi.AllocationBase);
    region.regionSize = mbi.RegionSize;
    region.state = memoryStateFromRaw(mbi.State);
    region.type = memoryTypeFromRaw(mbi.Type);
    region.protect = mbi.Protect;
    region.allocationProtect = mbi.AllocationProtect;
    return region;
}

// Resolves the user-mode address ceiling, falling back to the x64 default.
std::uint64_t resolveMaxUserAddress(const ntapi::INativeApi& native) {
    auto basic = native.querySystemBasicInformation();
    if (basic) {
        return static_cast<std::uint64_t>(basic.value().MaximumUserModeAddress);
    }
    return kDefaultMaxUserAddress;
}

}  // namespace

MemoryManager::MemoryManager(const ntapi::INativeApi& native) noexcept : native_(native) {}

Result<std::vector<MemoryRegion>, ErrorCode> MemoryManager::queryMap(std::uint32_t pid) const {
    auto handleResult = native_.openProcess(pid, kReadAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle process = std::move(handleResult).value();
    const std::uint64_t maxAddress = resolveMaxUserAddress(native_);

    std::vector<MemoryRegion> regions;
    std::uint64_t address = 0;

    while (address < maxAddress) {
        auto regionResult = native_.queryMemoryRegion(process.get(), address);
        if (!regionResult) {
            // STATUS_INVALID_PARAMETER past the boundary is the normal end of
            // the walk. Any other error also terminates it; whatever we have
            // collected so far is still a valid partial map.
            break;
        }

        const MEMORY_BASIC_INFORMATION mbi = regionResult.value();
        regions.push_back(toMemoryRegion(mbi));

        const std::uint64_t base = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
        const std::uint64_t next = base + mbi.RegionSize;
        if (next <= address) {
            break;  // guard against a zero-size region stalling the walk
        }
        address = next;
    }

    if (regions.empty()) {
        return makeUnexpected(ErrorCode::application("Memory map query returned no regions"));
    }
    return regions;
}

Result<std::vector<std::byte>, ErrorCode> MemoryManager::read(std::uint32_t pid,
                                                              std::uint64_t address,
                                                              std::size_t size) const {
    if (size == 0) {
        return std::vector<std::byte>{};
    }

    auto handleResult = native_.openProcess(pid, kReadAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle process = std::move(handleResult).value();

    std::vector<std::byte> buffer(size, std::byte{0});
    auto readResult = native_.readMemory(process.get(), address,
                                         MutableByteSpan(buffer.data(), buffer.size()));
    if (!readResult) {
        return makeUnexpected(std::move(readResult).error());
    }

    // Shrink to what was actually read (partial reads are expected near the end
    // of a committed range).
    buffer.resize(readResult.value());
    return buffer;
}

Result<std::vector<SignatureMatch>, ErrorCode> MemoryManager::scan(
    std::uint32_t pid, std::string_view pattern) const {
    auto scannerResult = SignatureScanner::compile(pattern);
    if (!scannerResult) {
        return makeUnexpected(std::move(scannerResult).error());
    }
    const SignatureScanner scanner = std::move(scannerResult).value();
    const std::size_t patternSize = scanner.size();

    auto mapResult = queryMap(pid);
    if (!mapResult) {
        return makeUnexpected(std::move(mapResult).error());
    }

    auto handleResult = native_.openProcess(pid, kReadAccess);
    if (!handleResult) {
        return makeUnexpected(std::move(handleResult).error());
    }
    const Handle process = std::move(handleResult).value();

    std::vector<SignatureMatch> matches;
    std::vector<std::byte> buffer;

    for (const MemoryRegion& region : mapResult.value()) {
        if (region.state != MemoryState::Committed || !isReadable(region.protect)) {
            continue;
        }

        std::uint64_t offset = 0;
        while (offset < region.regionSize) {
            const std::uint64_t remaining = region.regionSize - offset;
            const std::size_t chunkSize =
                static_cast<std::size_t>(remaining < kScanChunk ? remaining : kScanChunk);

            buffer.assign(chunkSize, std::byte{0});
            auto readResult = native_.readMemory(process.get(), region.baseAddress + offset,
                                                 MutableByteSpan(buffer.data(), buffer.size()));
            if (!readResult || readResult.value() == 0) {
                break;  // region became unreadable; skip the rest of it
            }
            const std::size_t bytesRead = readResult.value();

            const ByteSpan window(buffer.data(), bytesRead);
            for (const std::size_t hit : scanner.findAll(window)) {
                matches.push_back(SignatureMatch{region.baseAddress + offset + hit});
            }

            // Advance with overlap so a match straddling the window boundary is
            // still found on the next iteration.
            std::size_t step = bytesRead;
            if (bytesRead == chunkSize && remaining > chunkSize && patternSize > 1) {
                step = bytesRead - (patternSize - 1);
            }
            if (step == 0) {
                step = bytesRead;  // never stall
            }
            offset += step;
        }
    }

    return matches;
}

}  // namespace wis::core
