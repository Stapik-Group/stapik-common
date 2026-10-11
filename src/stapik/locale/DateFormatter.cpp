#include "DateFormatter.hpp"

#include "LanguageCode.hpp"

#include <ctime>
#include <format>

namespace stapik::locale
{
    namespace
    {
        constexpr std::string_view NO_BREAK_SPACE = "\xC2\xA0";

        struct DateConvention
        {
            bool monthFirst;
            char dateSeparator;
            bool twelveHourClock;
            std::string_view dateTimeSeparator;
        };

        DateConvention conventionFor(const std::string_view languageCode)
        {
            if (const auto language = primaryLanguage(languageCode); language == "pl" || language == "de")
                return {.monthFirst = false, .dateSeparator = '.', .twelveHourClock = false, .dateTimeSeparator = " "};

            return {.monthFirst = true, .dateSeparator = '/', .twelveHourClock = true, .dateTimeSeparator = ", "};
        }

        std::string formatClock(const std::chrono::hh_mm_ss<std::chrono::seconds>& clock, const bool twelveHourClock,
                                const bool withSeconds)
        {
            const auto hour = clock.hours().count();
            const auto minute = clock.minutes().count();
            const auto second = clock.seconds().count();

            if (!twelveHourClock)
            {
                return withSeconds
                           ? std::format("{:02}:{:02}:{:02}", hour, minute, second)
                           : std::format("{:02}:{:02}", hour, minute);
            }

            const auto displayHour = hour % 12 == 0 ? 12 : hour % 12;
            const auto meridiem = hour < 12 ? "AM" : "PM";
            return withSeconds
                       ? std::format("{}:{:02}:{:02}{}{}", displayHour, minute, second, NO_BREAK_SPACE, meridiem)
                       : std::format("{}:{:02}{}{}", displayHour, minute, NO_BREAK_SPACE, meridiem);
        }

        std::chrono::hh_mm_ss<std::chrono::seconds> timeOfDay(const LocalSeconds time)
        {
            return std::chrono::hh_mm_ss{time - std::chrono::floor<std::chrono::days>(time)};
        }
    }

    LocalSeconds toLocalTime(const std::chrono::system_clock::time_point timePoint)
    {
        using namespace std::chrono;

        const auto seconds = floor<std::chrono::seconds>(timePoint);
        const auto time = system_clock::to_time_t(seconds);

        std::tm broken{};
#ifdef _WIN32
        const bool converted = localtime_s(&broken, &time) == 0;
#else
        const bool converted = localtime_r(&time, &broken) != nullptr;
#endif
        if (!converted)
            return LocalSeconds{seconds.time_since_epoch()};

        const year_month_day date{
            year{broken.tm_year + 1900},
            month{static_cast<unsigned>(broken.tm_mon + 1)},
            day{static_cast<unsigned>(broken.tm_mday)}};

        return local_days{date} + hours{broken.tm_hour} + minutes{broken.tm_min} + std::chrono::seconds{broken.tm_sec};
    }

    std::string formatDate(const std::chrono::year_month_day date, const std::string_view languageCode)
    {
        const auto convention = conventionFor(languageCode);
        const auto year = static_cast<int>(date.year());
        const auto month = static_cast<unsigned>(date.month());
        const auto day = static_cast<unsigned>(date.day());
        const auto separator = convention.dateSeparator;

        if (convention.monthFirst)
            return std::format("{:02}{}{:02}{}{:04}", month, separator, day, separator, year);

        return std::format("{:02}{}{:02}{}{:04}", day, separator, month, separator, year);
    }

    std::string formatDate(const LocalSeconds time, const std::string_view languageCode)
    {
        return formatDate(std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(time)}, languageCode);
    }

    std::string formatTime(const LocalSeconds time, const std::string_view languageCode, const bool withSeconds)
    {
        return formatClock(timeOfDay(time), conventionFor(languageCode).twelveHourClock, withSeconds);
    }

    std::string formatDateTime(const LocalSeconds time, const std::string_view languageCode, const bool withSeconds)
    {
        return formatDate(time, languageCode) + std::string(conventionFor(languageCode).dateTimeSeparator)
               + formatTime(time, languageCode, withSeconds);
    }
}
