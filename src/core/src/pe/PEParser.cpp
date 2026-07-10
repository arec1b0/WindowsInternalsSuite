#include "core/pe/PEParser.hpp"

#include <windows.h>

#include <cstring>
#include <type_traits>
#include <utility>

namespace wis::core {
namespace {

// Bounds-checked read of a trivially-copyable struct at a file offset. Copies
// into `out` to avoid unaligned/aliasing hazards on untrusted input.
template <typename T>
[[nodiscard]] bool readAt(ByteSpan data, std::size_t offset, T& out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "readAt requires a POD type");
    if (offset > data.size() || data.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

// Extracts a section name from the fixed 8-byte, possibly non-terminated field.
[[nodiscard]] std::string sectionName(const BYTE (&raw)[IMAGE_SIZEOF_SHORT_NAME]) {
    std::size_t length = 0;
    while (length < IMAGE_SIZEOF_SHORT_NAME && raw[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(raw), length);
}

void fillOptional32(const IMAGE_OPTIONAL_HEADER32& raw, OptionalHeaderInfo& out) {
    out.magic = PeMagic::Pe32;
    out.addressOfEntryPoint = raw.AddressOfEntryPoint;
    out.imageBase = raw.ImageBase;
    out.sectionAlignment = raw.SectionAlignment;
    out.fileAlignment = raw.FileAlignment;
    out.sizeOfImage = raw.SizeOfImage;
    out.sizeOfHeaders = raw.SizeOfHeaders;
    out.subsystem = raw.Subsystem;
    out.dllCharacteristics = raw.DllCharacteristics;
    out.checkSum = raw.CheckSum;
    out.numberOfRvaAndSizes = raw.NumberOfRvaAndSizes;

    const std::uint32_t count = (raw.NumberOfRvaAndSizes < kNumberOfDataDirectories)
                                    ? raw.NumberOfRvaAndSizes
                                    : static_cast<std::uint32_t>(kNumberOfDataDirectories);
    for (std::uint32_t i = 0; i < count; ++i) {
        out.dataDirectories[i].virtualAddress = raw.DataDirectory[i].VirtualAddress;
        out.dataDirectories[i].size = raw.DataDirectory[i].Size;
    }
}

void fillOptional64(const IMAGE_OPTIONAL_HEADER64& raw, OptionalHeaderInfo& out) {
    out.magic = PeMagic::Pe32Plus;
    out.addressOfEntryPoint = raw.AddressOfEntryPoint;
    out.imageBase = raw.ImageBase;
    out.sectionAlignment = raw.SectionAlignment;
    out.fileAlignment = raw.FileAlignment;
    out.sizeOfImage = raw.SizeOfImage;
    out.sizeOfHeaders = raw.SizeOfHeaders;
    out.subsystem = raw.Subsystem;
    out.dllCharacteristics = raw.DllCharacteristics;
    out.checkSum = raw.CheckSum;
    out.numberOfRvaAndSizes = raw.NumberOfRvaAndSizes;

    const std::uint32_t count = (raw.NumberOfRvaAndSizes < kNumberOfDataDirectories)
                                    ? raw.NumberOfRvaAndSizes
                                    : static_cast<std::uint32_t>(kNumberOfDataDirectories);
    for (std::uint32_t i = 0; i < count; ++i) {
        out.dataDirectories[i].virtualAddress = raw.DataDirectory[i].VirtualAddress;
        out.dataDirectories[i].size = raw.DataDirectory[i].Size;
    }
}

}  // namespace

// --- Flag / enum formatters (declared in PEHeaders.hpp) ----------------------

std::string_view machineToString(std::uint16_t machine) noexcept {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386:  return "x86 (I386)";
        case IMAGE_FILE_MACHINE_AMD64: return "x64 (AMD64)";
        case IMAGE_FILE_MACHINE_ARM64: return "ARM64";
        case IMAGE_FILE_MACHINE_ARMNT: return "ARM (Thumb-2)";
        case IMAGE_FILE_MACHINE_IA64:  return "IA-64";
        case IMAGE_FILE_MACHINE_UNKNOWN: return "Unknown";
        default:                       return "Other";
    }
}

std::string_view subsystemToString(std::uint16_t subsystem) noexcept {
    switch (subsystem) {
        case IMAGE_SUBSYSTEM_NATIVE:       return "Native";
        case IMAGE_SUBSYSTEM_WINDOWS_GUI:  return "Windows GUI";
        case IMAGE_SUBSYSTEM_WINDOWS_CUI:  return "Windows CUI";
        case IMAGE_SUBSYSTEM_EFI_APPLICATION: return "EFI Application";
        case IMAGE_SUBSYSTEM_XBOX:         return "Xbox";
        case IMAGE_SUBSYSTEM_UNKNOWN:      return "Unknown";
        default:                           return "Other";
    }
}

std::string_view dataDirectoryName(std::size_t index) noexcept {
    switch (index) {
        case IMAGE_DIRECTORY_ENTRY_EXPORT:       return "Export";
        case IMAGE_DIRECTORY_ENTRY_IMPORT:       return "Import";
        case IMAGE_DIRECTORY_ENTRY_RESOURCE:     return "Resource";
        case IMAGE_DIRECTORY_ENTRY_EXCEPTION:    return "Exception";
        case IMAGE_DIRECTORY_ENTRY_SECURITY:     return "Security";
        case IMAGE_DIRECTORY_ENTRY_BASERELOC:    return "Base Relocation";
        case IMAGE_DIRECTORY_ENTRY_DEBUG:        return "Debug";
        case IMAGE_DIRECTORY_ENTRY_ARCHITECTURE: return "Architecture";
        case IMAGE_DIRECTORY_ENTRY_GLOBALPTR:    return "Global Pointer";
        case IMAGE_DIRECTORY_ENTRY_TLS:          return "TLS";
        case IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG:  return "Load Config";
        case IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT: return "Bound Import";
        case IMAGE_DIRECTORY_ENTRY_IAT:          return "IAT";
        case IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT: return "Delay Import";
        case IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR: return "CLR Runtime";
        default:                                 return "Reserved";
    }
}

std::string fileCharacteristicsToString(std::uint16_t characteristics) {
    std::string result;
    const auto append = [&](std::uint16_t flag, std::string_view label) {
        if ((characteristics & flag) != 0) {
            if (!result.empty()) {
                result += " | ";
            }
            result += label;
        }
    };
    append(IMAGE_FILE_RELOCS_STRIPPED, "RELOCS_STRIPPED");
    append(IMAGE_FILE_EXECUTABLE_IMAGE, "EXECUTABLE");
    append(IMAGE_FILE_LARGE_ADDRESS_AWARE, "LARGE_ADDRESS_AWARE");
    append(IMAGE_FILE_32BIT_MACHINE, "32BIT_MACHINE");
    append(IMAGE_FILE_DEBUG_STRIPPED, "DEBUG_STRIPPED");
    append(IMAGE_FILE_SYSTEM, "SYSTEM");
    append(IMAGE_FILE_DLL, "DLL");
    return result.empty() ? "-" : result;
}

std::string dllCharacteristicsToString(std::uint16_t characteristics) {
    std::string result;
    const auto append = [&](std::uint16_t flag, std::string_view label) {
        if ((characteristics & flag) != 0) {
            if (!result.empty()) {
                result += " | ";
            }
            result += label;
        }
    };
    append(IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA, "HIGH_ENTROPY_VA");
    append(IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE, "DYNAMIC_BASE (ASLR)");
    append(IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY, "FORCE_INTEGRITY");
    append(IMAGE_DLLCHARACTERISTICS_NX_COMPAT, "NX_COMPAT (DEP)");
    append(IMAGE_DLLCHARACTERISTICS_NO_SEH, "NO_SEH");
    append(IMAGE_DLLCHARACTERISTICS_GUARD_CF, "GUARD_CF");
    append(IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE, "TS_AWARE");
    return result.empty() ? "-" : result;
}

std::string sectionCharacteristicsToString(std::uint32_t characteristics) {
    std::string result;
    const auto append = [&](std::uint32_t flag, std::string_view label) {
        if ((characteristics & flag) != 0) {
            if (!result.empty()) {
                result += " | ";
            }
            result += label;
        }
    };
    append(IMAGE_SCN_CNT_CODE, "CODE");
    append(IMAGE_SCN_CNT_INITIALIZED_DATA, "INIT_DATA");
    append(IMAGE_SCN_CNT_UNINITIALIZED_DATA, "UNINIT_DATA");
    append(IMAGE_SCN_MEM_DISCARDABLE, "DISCARDABLE");
    append(IMAGE_SCN_MEM_SHARED, "SHARED");
    append(IMAGE_SCN_MEM_EXECUTE, "EXECUTE");
    append(IMAGE_SCN_MEM_READ, "READ");
    append(IMAGE_SCN_MEM_WRITE, "WRITE");
    return result.empty() ? "-" : result;
}

// --- Parser ------------------------------------------------------------------

Result<PEFile, ErrorCode> PEParser::parseFile(std::wstring_view path) {
    auto mappedResult = MappedFile::open(path);
    if (!mappedResult) {
        return makeUnexpected(std::move(mappedResult).error());
    }
    MappedFile mapped = std::move(mappedResult).value();
    const ByteSpan data = mapped.bytes();

    // 1. DOS header.
    IMAGE_DOS_HEADER dosHeader{};
    if (!readAt(data, 0, dosHeader)) {
        return makeUnexpected(ErrorCode::application("PE: file smaller than DOS header"));
    }
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        return makeUnexpected(ErrorCode::application("PE: missing 'MZ' signature"));
    }

    DosHeaderInfo dosInfo;
    dosInfo.magic = dosHeader.e_magic;
    dosInfo.newHeaderOffset = static_cast<std::uint32_t>(dosHeader.e_lfanew);

    // 2. NT signature.
    const std::size_t ntOffset = static_cast<std::size_t>(dosHeader.e_lfanew);
    DWORD ntSignature = 0;
    if (!readAt(data, ntOffset, ntSignature)) {
        return makeUnexpected(ErrorCode::application("PE: NT header offset out of range"));
    }
    if (ntSignature != IMAGE_NT_SIGNATURE) {
        return makeUnexpected(ErrorCode::application("PE: missing 'PE\\0\\0' signature"));
    }

    // 3. File header (immediately after the 4-byte NT signature).
    const std::size_t fileHeaderOffset = ntOffset + sizeof(DWORD);
    IMAGE_FILE_HEADER fileHeader{};
    if (!readAt(data, fileHeaderOffset, fileHeader)) {
        return makeUnexpected(ErrorCode::application("PE: truncated file header"));
    }

    FileHeaderInfo fileInfo;
    fileInfo.machine = fileHeader.Machine;
    fileInfo.numberOfSections = fileHeader.NumberOfSections;
    fileInfo.timeDateStamp = fileHeader.TimeDateStamp;
    fileInfo.sizeOfOptionalHeader = fileHeader.SizeOfOptionalHeader;
    fileInfo.characteristics = fileHeader.Characteristics;

    // 4. Optional header: dispatch on the Magic word (PE32 vs PE32+).
    const std::size_t optionalOffset = fileHeaderOffset + sizeof(IMAGE_FILE_HEADER);
    WORD optionalMagic = 0;
    if (!readAt(data, optionalOffset, optionalMagic)) {
        return makeUnexpected(ErrorCode::application("PE: truncated optional header"));
    }

    OptionalHeaderInfo optionalInfo;
    if (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 raw{};
        if (!readAt(data, optionalOffset, raw)) {
            return makeUnexpected(ErrorCode::application("PE: truncated PE32 optional header"));
        }
        fillOptional32(raw, optionalInfo);
    } else if (optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 raw{};
        if (!readAt(data, optionalOffset, raw)) {
            return makeUnexpected(ErrorCode::application("PE: truncated PE32+ optional header"));
        }
        fillOptional64(raw, optionalInfo);
    } else {
        return makeUnexpected(ErrorCode::application("PE: unknown optional header magic"));
    }

    // 5. Section table (immediately after the optional header, per its declared
    //    size — not sizeof, which can differ from what the file records).
    const std::size_t sectionTableOffset =
        optionalOffset + fileHeader.SizeOfOptionalHeader;
    const std::size_t sectionCount = fileHeader.NumberOfSections;
    const std::size_t sectionBytes = sectionCount * sizeof(IMAGE_SECTION_HEADER);

    if (sectionTableOffset > data.size() ||
        data.size() - sectionTableOffset < sectionBytes) {
        return makeUnexpected(ErrorCode::application("PE: section table out of range"));
    }

    std::vector<SectionInfo> sections;
    sections.reserve(sectionCount);
    for (std::size_t i = 0; i < sectionCount; ++i) {
        IMAGE_SECTION_HEADER raw{};
        const std::size_t offset = sectionTableOffset + i * sizeof(IMAGE_SECTION_HEADER);
        if (!readAt(data, offset, raw)) {
            return makeUnexpected(ErrorCode::application("PE: truncated section header"));
        }

        SectionInfo section;
        section.name = sectionName(raw.Name);
        section.virtualSize = raw.Misc.VirtualSize;
        section.virtualAddress = raw.VirtualAddress;
        section.sizeOfRawData = raw.SizeOfRawData;
        section.pointerToRawData = raw.PointerToRawData;
        section.characteristics = raw.Characteristics;
        sections.push_back(std::move(section));
    }

    return PEFile(std::move(mapped), dosInfo, fileInfo, optionalInfo, std::move(sections));
}

}  // namespace wis::core
