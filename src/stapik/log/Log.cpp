#include "Log.hpp"

#include "ConsoleSink.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>

namespace
{
    stapik::log::Level levelFromEnvironment()
    {
        const auto* value = std::getenv("STAPIK_LOG_LEVEL");
        if (value == nullptr)
            return stapik::log::Level::Info;

        return stapik::log::parseLevel(value).value_or(stapik::log::Level::Info);
    }

    std::mutex& sinksMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::vector<std::shared_ptr<stapik::log::ILogSink>>& sinkList()
    {
        static std::vector<std::shared_ptr<stapik::log::ILogSink>> list {std::make_shared<stapik::log::ConsoleSink>()};
        return list;
    }

    // Guarded by sinksMutex().
    std::shared_ptr<const stapik::time::IClock>& clockOverride()
    {
        static std::shared_ptr<const stapik::time::IClock> clock;
        return clock;
    }

    std::atomic<stapik::log::Level>& currentLevel()
    {
        static std::atomic level {levelFromEnvironment()};
        return level;
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

    void addSink(std::shared_ptr<ILogSink> sink)
    {
        const std::lock_guard lock(sinksMutex());
        sinkList().push_back(std::move(sink));
    }

    std::vector<std::shared_ptr<ILogSink>> sinks()
    {
        const std::lock_guard lock(sinksMutex());
        return sinkList();
    }

    void setSinks(std::vector<std::shared_ptr<ILogSink>> newSinks)
    {
        const std::lock_guard lock(sinksMutex());
        sinkList() = std::move(newSinks);
    }

    void setClock(std::shared_ptr<const time::IClock> clock)
    {
        const std::lock_guard lock(sinksMutex());
        clockOverride() = std::move(clock);
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

            const std::lock_guard lock(sinksMutex());

            const auto& clock = clockOverride() ? *clockOverride() : time::systemClock();
            const LogRecord record {messageLevel, message, clock.now()};

            for (const auto& sink : sinkList())
                sink->write(record);
        }
    }
}
