#pragma once

// OS-independent formatting helpers shared by view models. Anything requiring a
// Win32 call (timestamps, locale-aware text) belongs in the views instead.

#include <cstdint>
#include <cstdio>
#include <string>

namespace wis::ui::detail {

// Byte count with a binary-prefix unit, e.g. "184.2 MB".
[[nodiscard]] inline std::string formatBytes(std::uint64_t bytes) {
    constexpr const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    constexpr std::size_t unitCount = sizeof(units) / sizeof(units[0]);

    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < unitCount) {
        value /= 1024.0;
        ++unit;
    }

    char buffer[64] = {};
    if (unit == 0) {
        std::snprintf(buffer, sizeof(buffer), "%llu B",
                      static_cast<unsigned long long>(bytes));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);
    }
    return std::string(buffer);
}

// Percentage with one decimal, e.g. "12.4".
[[nodiscard]] inline std::string formatPercent(double percent) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f", percent);
    return std::string(buffer);
}

// 64-bit address as a fixed-width hex string, e.g. "00007FFA1B2C3D40".
[[nodiscard]] inline std::string formatAddress(std::uint64_t address) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%016llX",
                  static_cast<unsigned long long>(address));
    return std::string(buffer);
}

// Cumulative 100 ns CPU counter as "h:mm:ss.mmm".
[[nodiscard]] inline std::string formatCpuTime(std::uint64_t time100ns) {
    const std::uint64_t totalMs = time100ns / 10000;
    const std::uint64_t ms = totalMs % 1000;
    const std::uint64_t totalSeconds = totalMs / 1000;
    const std::uint64_t seconds = totalSeconds % 60;
    const std::uint64_t minutes = (totalSeconds / 60) % 60;
    const std::uint64_t hours = totalSeconds / 3600;

    char buffer[48] = {};
    std::snprintf(buffer, sizeof(buffer), "%llu:%02llu:%02llu.%03llu",
                  static_cast<unsigned long long>(hours),
                  static_cast<unsigned long long>(minutes),
                  static_cast<unsigned long long>(seconds),
                  static_cast<unsigned long long>(ms));
    return std::string(buffer);
}

}  // namespace wis::ui::detail
