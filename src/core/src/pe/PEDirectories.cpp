#include "core/pe/PEDirectories.hpp"

#include <windows.h>
#include <wintrust.h>

#include <array>
#include <cstdio>
#include <utility>

#include "core/pe/detail/PEReader.hpp"

namespace wis::core {
namespace {

constexpr std::size_t kMaxTlsCallbacks = 4096;
constexpr std::size_t kMaxDebugEntries = 256;
constexpr std::size_t kMaxRelocBlocks = 65536;
constexpr std::size_t kMaxCertificates = 256;

// Reads a NUL-terminated ASCII string at a raw file offset (used for the PDB
// path in CodeView records, which are addressed by PointerToRawData).
std::string readCStringAtOffset(ByteSpan data, std::size_t offset,
                                std::size_t maxLength = 4096) {
    std::string result;
    for (std::size_t i = 0; i < maxLength; ++i) {
        const std::size_t pos = offset + i;
        if (pos >= data.size()) {
            break;
        }
        const auto ch = static_cast<char>(data[pos]);
        if (ch == '\0') {
            break;
        }
        result.push_back(ch);
    }
    return result;
}

// Formats a 16-byte CodeView GUID as {8-4-4-4-12}. The first three groups are
// little-endian; the last two are byte-ordered as stored.
std::string formatGuid(const std::uint8_t (&g)[16]) {
    const auto d1 = static_cast<std::uint32_t>(g[0] | (g[1] << 8) | (g[2] << 16) |
                                               (static_cast<std::uint32_t>(g[3]) << 24));
    const auto d2 = static_cast<std::uint16_t>(g[4] | (g[5] << 8));
    const auto d3 = static_cast<std::uint16_t>(g[6] | (g[7] << 8));

    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer),
                  "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", d1, d2, d3,
                  g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
    return std::string(buffer);
}

std::string_view debugTypeName(std::uint32_t type) noexcept {
    switch (type) {
        case IMAGE_DEBUG_TYPE_UNKNOWN:       return "UNKNOWN";
        case IMAGE_DEBUG_TYPE_COFF:          return "COFF";
        case IMAGE_DEBUG_TYPE_CODEVIEW:      return "CODEVIEW";
        case IMAGE_DEBUG_TYPE_FPO:           return "FPO";
        case IMAGE_DEBUG_TYPE_MISC:          return "MISC";
        case IMAGE_DEBUG_TYPE_EXCEPTION:     return "EXCEPTION";
        case IMAGE_DEBUG_TYPE_FIXUP:         return "FIXUP";
        case IMAGE_DEBUG_TYPE_BORLAND:       return "BORLAND";
        case 13:                             return "POGO";
        case 14:                             return "ILTCG";
        case 16:                             return "REPRO";
        default:                             return "OTHER";
    }
}

std::string_view certificateTypeName(std::uint16_t type) noexcept {
    switch (type) {
        case WIN_CERT_TYPE_X509:             return "X509";
        case WIN_CERT_TYPE_PKCS_SIGNED_DATA: return "PKCS_SIGNED_DATA";
        case WIN_CERT_TYPE_RESERVED_1:       return "RESERVED_1";
        case WIN_CERT_TYPE_TS_STACK_SIGNED:  return "TS_STACK_SIGNED";
        default:                             return "UNKNOWN";
    }
}

// CodeView RSDS record layout: DWORD 'RSDS'; GUID; DWORD Age; char Path[].
struct CvInfoRsds {
    std::uint32_t signature;
    std::uint8_t guid[16];
    std::uint32_t age;
    // Path follows inline.
};

}  // namespace

// --- TLS ---------------------------------------------------------------------

Result<TlsInfo, ErrorCode> parseTls(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_TLS);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return TlsInfo{};  // no TLS
    }

    TlsInfo info;
    info.present = true;
    const std::uint64_t imageBase = pe.imageBase();

    if (pe.is64Bit()) {
        auto raw = detail::readAtRva<IMAGE_TLS_DIRECTORY64>(pe, directory->virtualAddress);
        if (!raw) {
            return makeUnexpected(ErrorCode::application("PE: unreadable TLS directory"));
        }
        info.startAddressOfRawData = raw->StartAddressOfRawData;
        info.endAddressOfRawData = raw->EndAddressOfRawData;
        info.addressOfIndex = raw->AddressOfIndex;
        info.addressOfCallBacks = raw->AddressOfCallBacks;
        info.sizeOfZeroFill = raw->SizeOfZeroFill;
        info.characteristics = raw->Characteristics;
    } else {
        auto raw = detail::readAtRva<IMAGE_TLS_DIRECTORY32>(pe, directory->virtualAddress);
        if (!raw) {
            return makeUnexpected(ErrorCode::application("PE: unreadable TLS directory"));
        }
        info.startAddressOfRawData = raw->StartAddressOfRawData;
        info.endAddressOfRawData = raw->EndAddressOfRawData;
        info.addressOfIndex = raw->AddressOfIndex;
        info.addressOfCallBacks = raw->AddressOfCallBacks;
        info.sizeOfZeroFill = raw->SizeOfZeroFill;
        info.characteristics = raw->Characteristics;
    }

    // AddressOfCallBacks is a VA pointing at a null-terminated array of callback
    // VAs. Convert VA -> RVA before reading from the on-disk image.
    if (info.addressOfCallBacks > imageBase) {
        const std::uint32_t callbacksRva =
            static_cast<std::uint32_t>(info.addressOfCallBacks - imageBase);
        const std::uint32_t pointerSize =
            pe.is64Bit() ? sizeof(std::uint64_t) : sizeof(std::uint32_t);

        for (std::size_t i = 0; i < kMaxTlsCallbacks; ++i) {
            const std::uint32_t slotRva =
                callbacksRva + static_cast<std::uint32_t>(i * pointerSize);

            std::uint64_t callback = 0;
            if (pe.is64Bit()) {
                auto value = detail::readAtRva<std::uint64_t>(pe, slotRva);
                if (!value) {
                    break;
                }
                callback = *value;
            } else {
                auto value = detail::readAtRva<std::uint32_t>(pe, slotRva);
                if (!value) {
                    break;
                }
                callback = *value;
            }
            if (callback == 0) {
                break;  // null terminator ends the callback array
            }
            info.callbacks.push_back(callback);
        }
    }

    return info;
}

// --- Debug -------------------------------------------------------------------

Result<std::vector<DebugEntry>, ErrorCode> parseDebug(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_DEBUG);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return std::vector<DebugEntry>{};  // no debug directory
    }

    const std::size_t count = directory->size / sizeof(IMAGE_DEBUG_DIRECTORY);
    if (count > kMaxDebugEntries) {
        return makeUnexpected(ErrorCode::application("PE: debug directory size out of range"));
    }

    std::vector<DebugEntry> entries;
    entries.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t entryRva =
            directory->virtualAddress +
            static_cast<std::uint32_t>(i * sizeof(IMAGE_DEBUG_DIRECTORY));
        auto raw = detail::readAtRva<IMAGE_DEBUG_DIRECTORY>(pe, entryRva);
        if (!raw) {
            break;
        }

        DebugEntry entry;
        entry.type = raw->Type;
        entry.typeName = std::string(debugTypeName(raw->Type));
        entry.timeDateStamp = raw->TimeDateStamp;
        entry.sizeOfData = raw->SizeOfData;
        entry.addressOfRawData = raw->AddressOfRawData;
        entry.pointerToRawData = raw->PointerToRawData;

        // Decode the CodeView RSDS record (PDB path + GUID + age) via the file
        // offset, which is reliable for an on-disk image.
        if (raw->Type == IMAGE_DEBUG_TYPE_CODEVIEW && raw->PointerToRawData != 0) {
            if (auto cv = detail::readAtOffset<CvInfoRsds>(pe.bytes(),
                                                           raw->PointerToRawData)) {
                // 'RSDS' == 0x53445352 little-endian.
                if (cv->signature == 0x53445352U) {
                    entry.hasCodeView = true;
                    entry.pdbGuid = formatGuid(cv->guid);
                    entry.pdbAge = cv->age;
                    entry.pdbPath = readCStringAtOffset(
                        pe.bytes(), raw->PointerToRawData + sizeof(CvInfoRsds));
                }
            }
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

// --- Base Relocations --------------------------------------------------------

std::string_view relocationTypeName(std::uint32_t type) noexcept {
    switch (type) {
        case IMAGE_REL_BASED_ABSOLUTE:       return "ABSOLUTE";
        case IMAGE_REL_BASED_HIGH:           return "HIGH";
        case IMAGE_REL_BASED_LOW:            return "LOW";
        case IMAGE_REL_BASED_HIGHLOW:        return "HIGHLOW";
        case IMAGE_REL_BASED_HIGHADJ:        return "HIGHADJ";
        case IMAGE_REL_BASED_DIR64:          return "DIR64";
        default:                             return "OTHER";
    }
}

Result<RelocationSummary, ErrorCode> parseRelocations(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_BASERELOC);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return RelocationSummary{};  // no relocations (e.g. RELOCS_STRIPPED)
    }

    RelocationSummary summary;
    summary.present = true;

    const std::uint32_t regionStart = directory->virtualAddress;
    const std::uint32_t regionEnd = regionStart + directory->size;
    std::uint32_t cursor = regionStart;

    for (std::size_t block = 0; block < kMaxRelocBlocks && cursor < regionEnd; ++block) {
        auto header = detail::readAtRva<IMAGE_BASE_RELOCATION>(pe, cursor);
        if (!header) {
            break;
        }
        // A block must be at least the header size; a zero/short block ends it.
        if (header->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) {
            break;
        }

        RelocationBlock relBlock;
        relBlock.pageRva = header->VirtualAddress;

        const std::uint32_t entriesBytes =
            header->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION);
        const std::uint32_t entryCount = entriesBytes / sizeof(std::uint16_t);
        relBlock.entryCount = entryCount;

        const std::uint32_t entriesRva = cursor + sizeof(IMAGE_BASE_RELOCATION);
        for (std::uint32_t e = 0; e < entryCount; ++e) {
            auto entry = detail::readAtRva<std::uint16_t>(
                pe, entriesRva + e * sizeof(std::uint16_t));
            if (!entry) {
                break;
            }
            const std::uint32_t type = (*entry >> 12) & 0x0FU;
            ++relBlock.typeCounts[type];
        }

        summary.totalEntries += entryCount;
        summary.blocks.push_back(relBlock);

        cursor += header->SizeOfBlock;  // advance to the next block
    }

    return summary;
}

// --- Certificate (Security) --------------------------------------------------

Result<CertificateInfo, ErrorCode> parseCertificates(const PEFile& pe) {
    const auto directory = pe.dataDirectory(IMAGE_DIRECTORY_ENTRY_SECURITY);
    if (!directory || directory->virtualAddress == 0 || directory->size == 0) {
        return CertificateInfo{};  // unsigned image
    }

    CertificateInfo info;
    info.present = true;
    info.tableOffset = directory->virtualAddress;  // NOTE: this is a FILE OFFSET
    info.tableSize = directory->size;

    // CRITICAL: the Security directory's VirtualAddress is NOT an RVA. It is a
    // raw file offset to a WIN_CERTIFICATE table. Read it directly from the
    // mapped bytes; do NOT route it through rvaToOffset.
    const ByteSpan data = pe.bytes();
    const std::uint32_t tableEnd = info.tableOffset + info.tableSize;
    std::uint32_t cursor = info.tableOffset;

    for (std::size_t i = 0; i < kMaxCertificates && cursor < tableEnd; ++i) {
        // WIN_CERTIFICATE: DWORD dwLength; WORD wRevision; WORD wCertificateType;
        // BYTE bCertificate[]. dwLength counts the whole structure incl. header.
        auto length = detail::readAtOffset<std::uint32_t>(data, cursor);
        auto revision = detail::readAtOffset<std::uint16_t>(data, cursor + 4);
        auto certType = detail::readAtOffset<std::uint16_t>(data, cursor + 6);
        if (!length || !revision || !certType) {
            break;
        }
        if (*length < 8) {
            break;  // malformed: length cannot be smaller than the header
        }

        CertificateEntry entry;
        entry.length = *length;
        entry.revision = *revision;
        entry.certificateType = *certType;
        entry.certificateTypeName = std::string(certificateTypeName(*certType));
        entry.dataOffset = cursor + 8;             // bCertificate[] starts here
        entry.dataSize = *length - 8;
        info.entries.push_back(std::move(entry));

        // Entries are 8-byte aligned (WIN_CERTIFICATE_REVISION quadword align).
        std::uint32_t next = cursor + *length;
        next = (next + 7U) & ~7U;
        if (next <= cursor) {
            break;  // guard against a stalling zero-length advance
        }
        cursor = next;
    }

    return info;
}

}  // namespace wis::core
