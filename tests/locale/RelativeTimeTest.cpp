#include "stapik/locale/LocalizationEngine.hpp"
#include "stapik/locale/RelativeTime.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    using stapik::locale::describeRelativeTime;
    using stapik::locale::formatRelativeTime;
    using stapik::locale::LocalSeconds;
    using stapik::locale::RelativeTimeKind;
    using stapik::locale::RelativeTimeOptions;

    const std::filesystem::path RESOURCES_DIR = STAPIK_COMMON_RESOURCES_DIR;

    LocalSeconds at(const year_month_day date, const hours hour = 12h, const minutes minute = 0min,
                    const seconds second = 0s)
    {
        return local_days{date} + hour + minute + second;
    }

    const auto NOW = at(2026y / October / 1, 12h);

    LocalSeconds ago(const seconds distance)
    {
        return NOW - distance;
    }

    LocalSeconds ahead(const seconds distance)
    {
        return NOW + distance;
    }

    LocalSeconds daysAgo(const int days)
    {
        return NOW - std::chrono::days{days};
    }

    LocalSeconds daysAhead(const int days)
    {
        return NOW + std::chrono::days{days};
    }

    void expectDescribed(const LocalSeconds when, const RelativeTimeKind kind, const long long count = 0)
    {
        const auto [actualKind, actualCount] = describeRelativeTime(when, NOW);
        EXPECT_EQ(actualKind, kind);
        EXPECT_EQ(actualCount, count);
    }

    std::string formatted(const LocalSeconds when, const std::string& language)
    {
        LocalizationEngine engine(RESOURCES_DIR / "locales");
        engine.setLanguage(language);
        return formatRelativeTime(when, NOW, engine);
    }
}

TEST(RelativeTimeTest, LessThanAMinuteIsJustNowOrSoon)
{
    expectDescribed(NOW, RelativeTimeKind::JustNow);
    expectDescribed(ago(59s), RelativeTimeKind::JustNow);
    expectDescribed(ahead(59s), RelativeTimeKind::Soon);
}

TEST(RelativeTimeTest, LessThanAnHourIsCountedInWholeMinutes)
{
    expectDescribed(ago(60s), RelativeTimeKind::MinutesAgo, 1);
    expectDescribed(ago(5min + 59s), RelativeTimeKind::MinutesAgo, 5);
    expectDescribed(ago(59min + 59s), RelativeTimeKind::MinutesAgo, 59);
    expectDescribed(ahead(90s), RelativeTimeKind::MinutesAhead, 1);
}

TEST(RelativeTimeTest, LessThanADayIsCountedInWholeHours)
{
    expectDescribed(ago(1h), RelativeTimeKind::HoursAgo, 1);
    expectDescribed(ago(23h + 59min), RelativeTimeKind::HoursAgo, 23);
    expectDescribed(ahead(2h + 30min), RelativeTimeKind::HoursAhead, 2);
}

TEST(RelativeTimeTest, HoursIgnoreTheCalendarDay)
{
    const auto shortlyAfterMidnight = at(2026y / October / 1, 1h);

    const auto [kind, count] = describeRelativeTime(at(2026y / September / 30, 23h), shortlyAfterMidnight);

    EXPECT_EQ(kind, RelativeTimeKind::HoursAgo);
    EXPECT_EQ(count, 2);
}

TEST(RelativeTimeTest, ADayAwayIsYesterdayOrTomorrow)
{
    expectDescribed(daysAgo(1), RelativeTimeKind::Yesterday, 1);
    expectDescribed(daysAhead(1), RelativeTimeKind::Tomorrow, 1);
    expectDescribed(at(2026y / September / 30, 8h), RelativeTimeKind::Yesterday, 1);
}

TEST(RelativeTimeTest, FurtherDaysAreCountedByCalendarDays)
{
    expectDescribed(daysAgo(3), RelativeTimeKind::DaysAgo, 3);
    expectDescribed(daysAhead(3), RelativeTimeKind::DaysAhead, 3);

    const auto justAfterMidnight = at(2026y / October / 1, 0h, 30min);
    const auto [kind, count] = describeRelativeTime(at(2026y / September / 29, 23h, 50min), justAfterMidnight);

    EXPECT_EQ(kind, RelativeTimeKind::DaysAgo);
    EXPECT_EQ(count, 2);
}

TEST(RelativeTimeTest, SixDaysAreRelativeAndSevenAreADate)
{
    expectDescribed(daysAgo(6), RelativeTimeKind::DaysAgo, 6);
    expectDescribed(daysAgo(7), RelativeTimeKind::Absolute);
    expectDescribed(daysAhead(6), RelativeTimeKind::DaysAhead, 6);
    expectDescribed(daysAhead(7), RelativeTimeKind::Absolute);
}

TEST(RelativeTimeTest, MaxRelativeDaysMovesTheLimit)
{
    constexpr RelativeTimeOptions options{.maxRelativeDays = 2};

    EXPECT_EQ(describeRelativeTime(daysAgo(2), NOW, options).kind, RelativeTimeKind::DaysAgo);
    EXPECT_EQ(describeRelativeTime(daysAgo(3), NOW, options).kind, RelativeTimeKind::Absolute);
    EXPECT_EQ(describeRelativeTime(daysAgo(1), NOW, options).kind, RelativeTimeKind::Yesterday);
}

TEST(RelativeTimeTest, PolishUsesPluralFormsOfTheNumber)
{
    EXPECT_EQ(formatted(ago(1min), "pl"), "1 minutę temu");
    EXPECT_EQ(formatted(ago(3min), "pl"), "3 minuty temu");
    EXPECT_EQ(formatted(ago(5min), "pl"), "5 minut temu");
    EXPECT_EQ(formatted(ago(22min), "pl"), "22 minuty temu");
    EXPECT_EQ(formatted(ago(1h), "pl"), "1 godzinę temu");
    EXPECT_EQ(formatted(ahead(2h), "pl"), "za 2 godziny");
    EXPECT_EQ(formatted(ahead(5h), "pl"), "za 5 godzin");
    EXPECT_EQ(formatted(daysAgo(3), "pl"), "3 dni temu");
    EXPECT_EQ(formatted(daysAhead(5), "pl"), "za 5 dni");
}

TEST(RelativeTimeTest, PolishNamesNearbyDays)
{
    EXPECT_EQ(formatted(NOW, "pl"), "przed chwilą");
    EXPECT_EQ(formatted(ahead(10s), "pl"), "za chwilę");
    EXPECT_EQ(formatted(daysAgo(1), "pl"), "wczoraj");
    EXPECT_EQ(formatted(daysAhead(1), "pl"), "jutro");
}

TEST(RelativeTimeTest, EnglishAndGermanHaveSingularAndPlural)
{
    EXPECT_EQ(formatted(ago(1min), "en"), "1 minute ago");
    EXPECT_EQ(formatted(ago(2h), "en"), "2 hours ago");
    EXPECT_EQ(formatted(ahead(3min), "en"), "in 3 minutes");
    EXPECT_EQ(formatted(daysAgo(1), "en"), "yesterday");
    EXPECT_EQ(formatted(daysAgo(4), "en"), "4 days ago");

    EXPECT_EQ(formatted(ago(1min), "de"), "vor 1 Minute");
    EXPECT_EQ(formatted(ago(2h), "de"), "vor 2 Stunden");
    EXPECT_EQ(formatted(daysAhead(1), "de"), "morgen");
    EXPECT_EQ(formatted(daysAhead(4), "de"), "in 4 Tagen");
}

TEST(RelativeTimeTest, TimesBeyondTheLimitAreShownAsADateInTheLanguageOfTheUser)
{
    EXPECT_EQ(formatted(daysAgo(11), "pl"), "20.09.2026");
    EXPECT_EQ(formatted(daysAgo(11), "en"), "09/20/2026");
}
