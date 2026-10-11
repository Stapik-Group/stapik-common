#pragma once

#include <string_view>

namespace stapik::log
{
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

    [[nodiscard]] constexpr std::string_view levelName(const Level level)
    {
        switch (level)
        {
            using enum Level;
            case Debug: return "DEBUG";
            case Info: return "INFO";
            case Warning: return "WARN";
            case Error: return "ERROR";
        }

        return "UNKNOWN";
    }
}
