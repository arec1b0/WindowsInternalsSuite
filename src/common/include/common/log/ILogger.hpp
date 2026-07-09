#pragma once

// Logging abstraction (DIP): core/ui depend on this interface, never on a
// concrete sink. Convenience methods forward to the single virtual log().

#include <string_view>

#include "common/log/LogLevel.hpp"

namespace wis {

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, std::string_view message) = 0;

    void trace(std::string_view message) { log(LogLevel::Trace, message); }
    void debug(std::string_view message) { log(LogLevel::Debug, message); }
    void info(std::string_view message) { log(LogLevel::Info, message); }
    void warning(std::string_view message) { log(LogLevel::Warning, message); }
    void error(std::string_view message) { log(LogLevel::Error, message); }
    void critical(std::string_view message) { log(LogLevel::Critical, message); }
};

}  // namespace wis