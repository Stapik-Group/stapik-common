#include "RelativeTime.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/locale/LocalizationEngine.hpp"

#include <string_view>

namespace stapik::locale
{
    namespace
    {
        struct MessageKey
        {
            std::string_view key;
            bool plural;
        };

        MessageKey messageKeyFor(const RelativeTimeKind kind)
        {
            using enum RelativeTimeKind;
            switch (kind)
            {
                case JustNow: return {.key = "time.relative.justNow", .plural = false};
                case Soon: return {.key = "time.relative.soon", .plural = false};
                case MinutesAgo: return {.key = "time.relative.minutesAgo", .plural = true};
                case MinutesAhead: return {.key = "time.relative.minutesAhead", .plural = true};
                case HoursAgo: return {.key = "time.relative.hoursAgo", .plural = true};
                case HoursAhead: return {.key = "time.relative.hoursAhead", .plural = true};
                case Yesterday: return {.key = "time.relative.yesterday", .plural = false};
                case Tomorrow: return {.key = "time.relative.tomorrow", .plural = false};
                case DaysAgo: return {.key = "time.relative.daysAgo", .plural = true};
                case DaysAhead: return {.key = "time.relative.daysAhead", .plural = true};
                case Absolute: break;
            }
            return {.key = "", .plural = false};
        }

        template<typename Translator>
        std::string render(const LocalSeconds when, const LocalSeconds now, const Translator& translations,
                           const RelativeTimeOptions options)
        {
            const auto [kind, count] = describeRelativeTime(when, now, options);
            if (kind == RelativeTimeKind::Absolute)
                return formatDate(when, translations.languageCode());

            const auto [key, plural] = messageKeyFor(kind);
            return plural ? translations.translatePlural(key, count) : translations.translate(key);
        }
    }

    RelativeTime describeRelativeTime(const LocalSeconds when, const LocalSeconds now, const RelativeTimeOptions options)
    {
        using namespace std::chrono;
        using enum RelativeTimeKind;

        const auto elapsed = now - when;
        const bool past = elapsed >= seconds::zero();
        const auto distance = past ? elapsed : -elapsed;

        if (distance < minutes{1})
            return {.kind = past ? JustNow : Soon, .count = 0};

        if (distance < hours{1})
            return {.kind = past ? MinutesAgo : MinutesAhead, .count = static_cast<long long>(duration_cast<minutes>(distance).count())};

        if (distance < days{1})
            return {.kind = past ? HoursAgo : HoursAhead, .count = static_cast<long long>(duration_cast<hours>(distance).count())};

        const auto calendarDays = (floor<days>(now) - floor<days>(when)).count();
        const auto calendarDistance = past ? calendarDays : -calendarDays;

        if (calendarDistance == 1)
            return {.kind = past ? Yesterday : Tomorrow, .count = 1};

        if (calendarDistance <= options.maxRelativeDays)
            return {.kind = past ? DaysAgo : DaysAhead, .count = static_cast<long long>(calendarDistance)};

        return {.kind = Absolute, .count = 0};
    }

    std::string formatRelativeTime(const LocalSeconds when, const LocalSeconds now,
                                   const LocalizationEngine& translations, const RelativeTimeOptions options)
    {
        return render(when, now, translations, options);
    }

    std::string formatRelativeTime(const LocalSeconds when, const LocalSeconds now, const LocaleManager& localeManager,
                                   const RelativeTimeOptions options)
    {
        return render(when, now, localeManager, options);
    }

    std::string formatRelativeTime(const std::chrono::system_clock::time_point when, const LocaleManager& localeManager,
                                   const RelativeTimeOptions options, const time::IClock& clock)
    {
        return render(toLocalTime(when), toLocalTime(clock.now()), localeManager, options);
    }
}
