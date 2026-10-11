#include "stapik/locale/DateFormatter.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    using stapik::locale::formatDate;
    using stapik::locale::formatDateTime;
    using stapik::locale::formatTime;
    using stapik::locale::LocalSeconds;
    using stapik::locale::toLocalTime;

    constexpr std::string NBSP = "\xC2\xA0";

    LocalSeconds at(const year_month_day date, const hours hour = 0h, const minutes minute = 0min,
                    const seconds second = 0s)
    {
        return local_days{date} + hour + minute + second;
    }

    const auto AFTERNOON = at(2026y / September / 30, 14h, 5min, 9s);
}

TEST(DateFormatterTest, PolishAndGermanPutTheDayFirst)
{
    EXPECT_EQ(formatDate(AFTERNOON, "pl"), "30.09.2026");
    EXPECT_EQ(formatDate(AFTERNOON, "de"), "30.09.2026");
}

TEST(DateFormatterTest, EnglishPutsTheMonthFirst)
{
    EXPECT_EQ(formatDate(AFTERNOON, "en"), "09/30/2026");
}

TEST(DateFormatterTest, UnknownLanguageFollowsEnglish)
{
    EXPECT_EQ(formatDate(AFTERNOON, "fr"), "09/30/2026");
    EXPECT_EQ(formatDate(AFTERNOON, ""), "09/30/2026");
}

TEST(DateFormatterTest, RegionAndCaseOfTheLanguageCodeAreIgnored)
{
    EXPECT_EQ(formatDate(AFTERNOON, "pl_PL.UTF-8"), "30.09.2026");
    EXPECT_EQ(formatDate(AFTERNOON, "DE-AT"), "30.09.2026");
}

TEST(DateFormatterTest, SingleDigitDayAndMonthAreZeroPadded)
{
    EXPECT_EQ(formatDate(2026y / March / 5, "pl"), "05.03.2026");
    EXPECT_EQ(formatDate(2026y / March / 5, "en"), "03/05/2026");
}

TEST(DateFormatterTest, EarlyYearsAreZeroPaddedToFourDigits)
{
    EXPECT_EQ(formatDate(year_month_day{5y / January / 2}, "pl"), "02.01.0005");
}

TEST(DateFormatterTest, TimeIsFormattedOnA24HourClockForPolishAndGerman)
{
    EXPECT_EQ(formatTime(AFTERNOON, "pl"), "14:05");
    EXPECT_EQ(formatTime(AFTERNOON, "de"), "14:05");
    EXPECT_EQ(formatTime(at(2026y / September / 30, 7h, 3min), "pl"), "07:03");
}

TEST(DateFormatterTest, TimeIsFormattedOnA12HourClockForEnglish)
{
    EXPECT_EQ(formatTime(AFTERNOON, "en"), "2:05" + NBSP + "PM");
    EXPECT_EQ(formatTime(at(2026y / September / 30, 7h, 3min), "en"), "7:03" + NBSP + "AM");
}

TEST(DateFormatterTest, TwelveHourClockHandlesMidnightAndNoon)
{
    EXPECT_EQ(formatTime(at(2026y / September / 30, 0h, 30min), "en"), "12:30" + NBSP + "AM");
    EXPECT_EQ(formatTime(at(2026y / September / 30, 12h), "en"), "12:00" + NBSP + "PM");
    EXPECT_EQ(formatTime(at(2026y / September / 30, 23h, 59min), "en"), "11:59" + NBSP + "PM");
}

TEST(DateFormatterTest, SecondsAreOptional)
{
    EXPECT_EQ(formatTime(AFTERNOON, "pl", true), "14:05:09");
    EXPECT_EQ(formatTime(AFTERNOON, "en", true), "2:05:09" + NBSP + "PM");
}

TEST(DateFormatterTest, DateTimeJoinsDateAndTime)
{
    EXPECT_EQ(formatDateTime(AFTERNOON, "pl"), "30.09.2026 14:05");
    EXPECT_EQ(formatDateTime(AFTERNOON, "de", true), "30.09.2026 14:05:09");
    EXPECT_EQ(formatDateTime(AFTERNOON, "en"), "09/30/2026, 2:05" + NBSP + "PM");
}

TEST(DateFormatterTest, TimesBeforeTheEpochAreFormattedAsCalendarTimes)
{
    EXPECT_EQ(formatDateTime(at(1969y / December / 31, 23h, 59min), "pl"), "31.12.1969 23:59");
}

TEST(DateFormatterTest, ToLocalTimeStaysCloseToTheUtcTime)
{
    const auto now = floor<seconds>(system_clock::now());
    const auto local = toLocalTime(now);

    const auto offset = local.time_since_epoch() - now.time_since_epoch();
    EXPECT_LE(offset, 15h);
    EXPECT_GE(offset, -15h);
}
