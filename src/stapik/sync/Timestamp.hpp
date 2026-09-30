#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace stapik::sync
{
    enum class TimestampPrecision
    {
        Seconds,
        Microseconds
    };

    [[nodiscard]] std::string toIso8601(std::chrono::system_clock::time_point tp, TimestampPrecision precision = TimestampPrecision::Seconds);
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> parseIso8601(std::string_view text);
    [[nodiscard]] std::chrono::system_clock::time_point fromIso8601(const std::string& str);
}