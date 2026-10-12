#include "stapik/domain/YearRange.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace stapik::domain;
using namespace std::chrono;

TEST(YearRangeTest, ConstructorRejectsInvalidRanges)
{
    EXPECT_THROW(YearRange(2027, 2026), std::invalid_argument);
    EXPECT_THROW(YearRange(0, 2026), std::invalid_argument);
    EXPECT_THROW(YearRange(2026, 10000), std::invalid_argument);
}

TEST(YearRangeTest, YearCountIsInclusive)
{
    EXPECT_EQ(YearRange(2026, 2026).yearCount(), 1);
    EXPECT_EQ(YearRange(2024, 2026).yearCount(), 3);
}

TEST(YearRangeTest, ContainsYearsAndMonths)
{
    const YearRange range(2024, 2026);
    EXPECT_TRUE(range.contains(2024));
    EXPECT_TRUE(range.contains(2026));
    EXPECT_FALSE(range.contains(2023));
    EXPECT_FALSE(range.contains(2027));
    EXPECT_TRUE(range.contains(YearMonth(2025, 7)));
    EXPECT_FALSE(range.contains(YearMonth(2027, 1)));
}

TEST(YearRangeTest, OverlapAndIntersection)
{
    const YearRange first(2020, 2024);
    const YearRange second(2024, 2026);
    const YearRange disjoint(2025, 2026);

    EXPECT_TRUE(first.overlaps(second));
    EXPECT_EQ(first.intersection(second), YearRange(2024, 2024));
    EXPECT_FALSE(first.overlaps(disjoint));
    EXPECT_FALSE(first.intersection(disjoint).has_value());
}

TEST(YearRangeTest, MonthsCoverWholeYears)
{
    const YearRange range(2025, 2026);
    const auto months = range.months();
    EXPECT_EQ(months.first(), YearMonth(2025, 1));
    EXPECT_EQ(months.last(), YearMonth(2026, 12));
    EXPECT_EQ(months.size(), 24);
}

TEST(YearRangeTest, DaysCoverWholeYears)
{
    const DateRange days = YearRange(2023, 2024).days();
    EXPECT_EQ(days.first(), 2023y / January / 1d);
    EXPECT_EQ(days.last(), 2024y / December / 31d);
    EXPECT_EQ(days.dayCount(), 365 + 366);
}
