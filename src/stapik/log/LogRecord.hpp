#pragma once

#include "LogLevel.hpp"

#include <chrono>
#include <string>

namespace stapik::log
{
    struct LogRecord
    {
        Level level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    // "2026-10-11 12:00:00 [WARN] message", the time in UTC, without a trailing newline.
    [[nodiscard]] std::string formatRecord(const LogRecord& record);
}
