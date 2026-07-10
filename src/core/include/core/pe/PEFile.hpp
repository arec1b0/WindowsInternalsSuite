#pragma once

// Parsed PE image container. Owns the mapped file and the decoded headers, and
// provides RVA translation used by the directory parsers (imports/exports/...).
// Methods are inline: they are small and pure, and this keeps the header the
// single source of the RVA logic shared across translation units.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "common/io/MappedFile.hpp"
#include "common/util/ByteSpan.hpp"
#include "core/pe/PEHeaders.hpp"
#include "core/pe/PESection.hpp"

namespace wis::core {

class PEFile {
public:
    PEFile() = default;

    PEFile(MappedFile file, DosHeaderInfo dos, FileHeaderInfo fileHeader,
           OptionalHeaderInfo optionalHeader, std::vector<SectionInfo> sections) noexcept
        : file_(std::move(file)),
          dos_(dos),
          fileHeader_(fileHeader),
          optional_(optionalHeader),
          sections_(std::move(sections)) {}

    [[nodiscard]] const DosHeaderInfo& dosHeader() const noexcept { return dos_; }
    [[nodiscard]] const FileHeaderInfo& fileHeader() const noexcept { return fileHeader_; }
    [[nodiscard]] const OptionalHeaderInfo& optionalHeader() const noexcept {
        return optional_;
    }
    [[nodiscard]] const std::vector<SectionInfo>& sections() const noexcept {
        return sections_;
    }

    [[nodiscard]] bool is64Bit() const noexcept {
        return optional_.magic == PeMagic::Pe32Plus;
    }
    [[nodiscard]] std::uint64_t imageBase() const noexcept { return optional_.imageBase; }

    // Whole-file byte view (for the hex viewer).
    [[nodiscard]] ByteSpan bytes() const noexcept { return file_.bytes(); }

    // Translates an RVA to a file offset. RVAs inside the header area map
    // directly; otherwise the containing section supplies the delta. Returns
    // nullopt when the RVA is not backed by any file bytes.
    [[nodiscard]] std::optional<std::size_t> rvaToOffset(std::uint32_t rva) const noexcept {
        const std::size_t fileSize = file_.size();

        // Header region: file offset == RVA up to SizeOfHeaders.
        if (rva < optional_.sizeOfHeaders) {
            return (rva < fileSize) ? std::optional<std::size_t>(rva) : std::nullopt;
        }

        for (const SectionInfo& section : sections_) {
            // A section's in-memory span is max(VirtualSize, SizeOfRawData).
            const std::uint32_t span = (section.virtualSize > section.sizeOfRawData)
                                           ? section.virtualSize
                                           : section.sizeOfRawData;
            if (rva >= section.virtualAddress &&
                rva < section.virtualAddress + span) {
                const std::uint32_t delta = rva - section.virtualAddress;
                // The RVA may lie in the section's uninitialized tail (beyond
                // SizeOfRawData); such bytes have no file backing.
                if (delta >= section.sizeOfRawData) {
                    return std::nullopt;
                }
                const std::size_t offset =
                    static_cast<std::size_t>(section.pointerToRawData) + delta;
                return (offset < fileSize) ? std::optional<std::size_t>(offset)
                                           : std::nullopt;
            }
        }
        return std::nullopt;
    }

    // Byte view of `size` bytes starting at `rva`, or an empty span when the
    // range is not fully backed by contiguous file data.
    [[nodiscard]] ByteSpan viewAtRva(std::uint32_t rva, std::size_t size) const noexcept {
        const auto offset = rvaToOffset(rva);
        if (!offset) {
            return {};
        }
        const ByteSpan whole = file_.bytes();
        if (*offset > whole.size() || whole.size() - *offset < size) {
            return {};
        }
        return whole.subspan(*offset, size);
    }

    // Data directory entry by index (bounds-checked against the header count).
    [[nodiscard]] std::optional<DataDirectoryInfo> dataDirectory(
        std::size_t index) const noexcept {
        if (index >= kNumberOfDataDirectories ||
            index >= optional_.numberOfRvaAndSizes) {
            return std::nullopt;
        }
        return optional_.dataDirectories[index];
    }

private:
    MappedFile file_;
    DosHeaderInfo dos_{};
    FileHeaderInfo fileHeader_{};
    OptionalHeaderInfo optional_{};
    std::vector<SectionInfo> sections_;
};

}  // namespace wis::core
