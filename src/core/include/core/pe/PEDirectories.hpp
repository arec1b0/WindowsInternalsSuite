#pragma once

// Parsers for the smaller PE data directories: TLS, Debug, Base Relocations,
// and the Certificate (Security) table. Each returns an empty/default result
// when its directory is absent (a valid, non-error outcome).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "common/error/ErrorCode.hpp"
#include "common/error/Result.hpp"
#include "core/pe/PEFile.hpp"

namespace wis::core {

// --- TLS ---------------------------------------------------------------------

struct TlsInfo {
    bool present = false;
    std::uint64_t startAddressOfRawData = 0;  // VA
    std::uint64_t endAddressOfRawData = 0;    // VA
    std::uint64_t addressOfIndex = 0;         // VA
    std::uint64_t addressOfCallBacks = 0;     // VA of the callback pointer array
    std::uint32_t sizeOfZeroFill = 0;
    std::uint32_t characteristics = 0;
    std::vector<std::uint64_t> callbacks;     // resolved callback VAs (may be empty)
};

[[nodiscard]] Result<TlsInfo, ErrorCode> parseTls(const PEFile& pe);

// --- Debug -------------------------------------------------------------------

struct DebugEntry {
    std::uint32_t type = 0;
    std::string typeName;          // e.g. "CODEVIEW", "POGO", "REPRO"
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfData = 0;
    std::uint32_t addressOfRawData = 0;  // RVA
    std::uint32_t pointerToRawData = 0;  // file offset

    // CodeView (RSDS) payload, populated when type == IMAGE_DEBUG_TYPE_CODEVIEW.
    bool hasCodeView = false;
    std::string pdbPath;
    std::string pdbGuid;           // formatted {xxxxxxxx-xxxx-...}
    std::uint32_t pdbAge = 0;
};

[[nodiscard]] Result<std::vector<DebugEntry>, ErrorCode> parseDebug(const PEFile& pe);

// --- Base Relocations --------------------------------------------------------

struct RelocationBlock {
    std::uint32_t pageRva = 0;
    std::uint32_t entryCount = 0;
    // Counts per relocation type, indexed by IMAGE_REL_BASED_* value.
    std::uint32_t typeCounts[16]{};
};

struct RelocationSummary {
    bool present = false;
    std::uint32_t totalEntries = 0;
    std::vector<RelocationBlock> blocks;
};

[[nodiscard]] std::string_view relocationTypeName(std::uint32_t type) noexcept;
[[nodiscard]] Result<RelocationSummary, ErrorCode> parseRelocations(const PEFile& pe);

// --- Certificate (Security) --------------------------------------------------

struct CertificateEntry {
    std::uint32_t length = 0;         // full WIN_CERTIFICATE length in bytes
    std::uint16_t revision = 0;
    std::uint16_t certificateType = 0;
    std::string certificateTypeName;  // e.g. "PKCS_SIGNED_DATA"
    std::uint32_t dataOffset = 0;     // file offset of bCertificate[]
    std::uint32_t dataSize = 0;       // size of bCertificate[] payload
};

struct CertificateInfo {
    bool present = false;
    std::uint32_t tableOffset = 0;    // file offset of the certificate table
    std::uint32_t tableSize = 0;
    std::vector<CertificateEntry> entries;
};

[[nodiscard]] Result<CertificateInfo, ErrorCode> parseCertificates(const PEFile& pe);

}  // namespace wis::core
