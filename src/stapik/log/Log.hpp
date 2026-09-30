#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace stapik::log
{
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

    [[nodiscard]] Level level();
    void setLevel(Level newLevel);

    [[nodiscard]] std::string redact(std::string_view secret);

    namespace detail
    {
        void write(Level messageLevel, const std::string& message);
    }

    template <typename... Args>
    void debug(std::format_string<Args...> format, Args&&... args)
    {
        detail::write(Level::Debug, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void info(std::format_string<Args...> format, Args&&... args)
    {
        detail::write(Level::Info, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warning(std::format_string<Args...> format, Args&&... args)
    {
        detail::write(Level::Warning, std::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void error(std::format_string<Args...> format, Args&&... args)
    {
        detail::write(Level::Error, std::format(format, std::forward<Args>(args)...));
    }
}