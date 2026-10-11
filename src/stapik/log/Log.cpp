#include "Log.hpp"

#include <glib.h>
#include <algorithm>
#include <atomic>
#include <cctype>

namespace
{
    constexpr auto LOG_DOMAIN = "stapik";

    stapik::log::Level levelFromEnvironment()
    {
        const auto* value = std::getenv("STAPIK_LOG_LEVEL");
        if (value == nullptr)
            return stapik::log::Level::Info;

        return stapik::log::parseLevel(value).value_or(stapik::log::Level::Info);
    }

    std::atomic<stapik::log::Level>& currentLevel()
    {
        static std::atomic level {levelFromEnvironment()};
        return level;
    }

    GLogLevelFlags toGLibLevel(const stapik::log::Level level)
    {
        using stapik::log::Level;

        switch (level)
        {
            using enum Level;
            case Debug: return G_LOG_LEVEL_DEBUG;
            case Info: return G_LOG_LEVEL_INFO;
            case Warning: return G_LOG_LEVEL_WARNING;
            case Error: return G_LOG_LEVEL_CRITICAL;
        }

        return G_LOG_LEVEL_MESSAGE;
    }
}

namespace stapik::log
{
    std::optional<Level> parseLevel(const std::string_view name)
    {
        std::string lowered(name);
        std::ranges::transform(lowered, lowered.begin(), [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

        if (lowered == "debug") return Level::Debug;
        if (lowered == "info") return Level::Info;
        if (lowered == "warning" || lowered == "warn") return Level::Warning;
        if (lowered == "error" || lowered == "err") return Level::Error;

        return std::nullopt;
    }

    Level level()
    {
        return currentLevel().load();
    }

    void setLevel(const Level newLevel)
    {
        currentLevel().store(newLevel);
    }

    std::string redact(const std::string_view secret)
    {
        constexpr std::size_t VISIBLE_CHARACTERS = 4;
        if (constexpr std::size_t MIN_LENGTH_TO_SHOW_PREFIX = 9; secret.size() < MIN_LENGTH_TO_SHOW_PREFIX)
            return "...";

        return std::string(secret.substr(0, VISIBLE_CHARACTERS)) + "...";
    }

    namespace detail
    {
        void write(const Level messageLevel, const std::string &message)
        {
            if (messageLevel < level())
                return;

            g_log(LOG_DOMAIN, toGLibLevel(messageLevel), "%s", message.c_str());
        }
    }
}
