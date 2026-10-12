#pragma once

#include "DateFormatter.hpp"

#include "stapik/time/Clock.hpp"

#include <chrono>
#include <string>

class LocaleManager;
class LocalizationEngine;

namespace stapik::locale
{
    enum class RelativeTimeKind
    {
        JustNow,
        Soon,
        MinutesAgo,
        MinutesAhead,
        HoursAgo,
        HoursAhead,
        Yesterday,
        Tomorrow,
        DaysAgo,
        DaysAhead,
        Absolute
    };

    struct RelativeTime
    {
        RelativeTimeKind kind;
        long long count;
    };

    struct RelativeTimeOptions
    {
        // Further away than this many calendar days, the time is shown as a date.
        int maxRelativeDays = 6;
    };

    // Less than a minute: just now. Less than a day: minutes or hours. Beyond that the calendar days decide:
    // yesterday / tomorrow, N days, and finally a date.
    [[nodiscard]] RelativeTime describeRelativeTime(LocalSeconds when, LocalSeconds now, RelativeTimeOptions options = {});

    [[nodiscard]] std::string formatRelativeTime(
        LocalSeconds when,
        LocalSeconds now,
        const LocalizationEngine& translations,
        RelativeTimeOptions options = {});

    [[nodiscard]] std::string formatRelativeTime(
        LocalSeconds when,
        LocalSeconds now,
        const LocaleManager& localeManager,
        RelativeTimeOptions options = {});

    // Relative to the current time of the clock.
    [[nodiscard]] std::string formatRelativeTime(
        std::chrono::system_clock::time_point when,
        const LocaleManager& localeManager,
        RelativeTimeOptions options = {},
        const time::IClock& clock = time::systemClock());
}
