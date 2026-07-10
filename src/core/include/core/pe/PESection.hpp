#pragma once

// Domain model for one PE section header. Retains the raw address/size fields
// needed for RVA translation plus the characteristics flag word.

#include <cstdint>
#include <string>

namespace wis::core {

struct SectionInfo {
    std::string name;                   // up to 8 chars (may be non-terminated)
    std::uint32_t virtualSize = 0;
    std::uint32_t virtualAddress = 0;   // RVA of the section in the image
    std::uint32_t sizeOfRawData = 0;
    std::uint32_t pointerToRawData = 0; // file offset of the section's bytes
    std::uint32_t characteristics = 0;
};

[[nodiscard]] std::string sectionCharacteristicsToString(std::uint32_t characteristics);

}  // namespace wis::core
