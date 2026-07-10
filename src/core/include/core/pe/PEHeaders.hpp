#pragma once

// Domain models for the PE header structures (DOS / File / Optional headers and
// the data directory table), normalized away from the raw IMAGE_* layout so the
// UI does not depend on winnt.h. Formatters for the various flag fields are
// declared here and implemented in PEParser.cpp.

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wis::core {

enum class PeMagic : std::uint16_t {
    Unknown = 0,
    Pe32 = 0x10B,      // 32-bit optional header
    Pe32Plus = 0x20B,  // 64-bit optional header (PE32+)
};

// Number of data directory entries in a standard optional header (16).
inline constexpr std::size_t kNumberOfDataDirectories = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

struct DosHeaderInfo {
    std::uint16_t magic = 0;            // 'MZ' == 0x5A4D
    std::uint32_t newHeaderOffset = 0;  // e_lfanew (offset to the NT headers)
};

struct FileHeaderInfo {
    std::uint16_t machine = 0;
    std::uint16_t numberOfSections = 0;
    std::uint32_t timeDateStamp = 0;    // seconds since 1970-01-01 UTC
    std::uint16_t sizeOfOptionalHeader = 0;
    std::uint16_t characteristics = 0;
};

struct DataDirectoryInfo {
    std::uint32_t virtualAddress = 0;   // RVA (0 == directory absent)
    std::uint32_t size = 0;
};

struct OptionalHeaderInfo {
    PeMagic magic = PeMagic::Unknown;
    std::uint32_t addressOfEntryPoint = 0;  // RVA
    std::uint64_t imageBase = 0;
    std::uint32_t sectionAlignment = 0;
    std::uint32_t fileAlignment = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t sizeOfHeaders = 0;
    std::uint16_t subsystem = 0;
    std::uint16_t dllCharacteristics = 0;
    std::uint32_t checkSum = 0;
    std::uint32_t numberOfRvaAndSizes = 0;
    DataDirectoryInfo dataDirectories[kNumberOfDataDirectories]{};
};

[[nodiscard]] std::string_view machineToString(std::uint16_t machine) noexcept;
[[nodiscard]] std::string_view subsystemToString(std::uint16_t subsystem) noexcept;
[[nodiscard]] std::string_view dataDirectoryName(std::size_t index) noexcept;
[[nodiscard]] std::string fileCharacteristicsToString(std::uint16_t characteristics);
[[nodiscard]] std::string dllCharacteristicsToString(std::uint16_t characteristics);

}  // namespace wis::core
