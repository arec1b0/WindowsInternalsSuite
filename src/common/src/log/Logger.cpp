#include "common/log/Logger.hpp"

#include <windows.h>

#include <format>
#include <iostream>

namespace wis {
namespace {

// Local timestamp via GetLocalTime to avoid locale/chrono-format surprises.
std::string timestamp() {
    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                       st.wYear, st.wMonth, st.wDay,
                       st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

}  // namespace

Logger::Logger(LogLevel minLevel, std::ostream* sink, bool mirrorToDebugger)
    : minLevel_(minLevel),
      sink_(sink != nullptr ? sink : &std::clog),
      mirrorToDebugger_(mirrorToDebugger) {}

void Logger::log(LogLevel level, std::string_view message) {
    if (level < minLevel_) {
        return;
    }

    const std::string line =
        std::format("[{}] [{}] {}\n", timestamp(), toString(level), message);

    std::scoped_lock lock(mutex_);
    (*sink_) << line;
    sink_->flush();

    if (mirrorToDebugger_) {
        ::OutputDebugStringA(line.c_str());
    }
}

}  // namespace wis