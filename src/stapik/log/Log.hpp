#pragma once

#include "ILogSink.hpp"
#include "LogLevel.hpp"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stapik::log
{
    [[nodiscard]] std::optional<Level> parseLevel(std::string_view name);
    [[nodiscard]] Level level();
    void setLevel(Level newLevel);

    void addSink(std::shared_ptr<ILogSink> sink);
    [[nodiscard]] std::vector<std::shared_ptr<ILogSink>> sinks();
    void setSinks(std::vector<std::shared_ptr<ILogSink>> newSinks);

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