#pragma once

// Default ILogger: thread-safe, writes formatted lines to an ostream and
// (optionally) to the debugger via OutputDebugString. Filters below minLevel.

#include <mutex>
#include <ostream>
#include <string_view>

#include "common/log/ILogger.hpp"

namespace wis {

class Logger : public ILogger {
public:
    // `sink` must outlive the logger (defaults to std::clog).
    explicit Logger(LogLevel minLevel = LogLevel::Info,
                    std::ostream* sink = nullptr,
                    bool mirrorToDebugger = true);

    void log(LogLevel level, std::string_view message) override;

    void setMinLevel(LogLevel level) noexcept { minLevel_ = level; }
    [[nodiscard]] LogLevel minLevel() const noexcept { return minLevel_; }

private:
    LogLevel minLevel_;
    std::ostream* sink_;
    bool mirrorToDebugger_;
    std::mutex mutex_;
};

}  // namespace wis