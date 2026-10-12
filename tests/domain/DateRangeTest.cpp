#include "stapik/domain/DateRange.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using namespace stapik::domain;
using namespace std::chrono;

TEST(DateRangeTest, ConstructorRejectsInvalidRanges)
{
    EXPECT_THROW(DateRange(2026y / March / 2d, 2026y / March / 1d), std::invalid_argument);
    EXPECT_THROW(DateRange(2026y / February / 30d, 2026y / March / 1d), std::invalid_argument);
    EXPECT_THROW(DateRange(2026y / March / 1d, 2026y / February / 31d), std::invalid_argument);
    EXPECT_THROW(DateRange(0y / January / 1d, 2026y / March / 1d), std::invalid_argument);
    EXPECT_THROW(DateRange(2026y / March / 1d, 10000y / January / 1d), std::invalid_argument);
}

TEST(DateRangeTest, SingleDayRangeIsValid)
{
    const DateRange range(2026y / March / 1d, 2026y / March / 1d);
    EXPECT_EQ(range.dayCount(), 1);
    EXPECT_TRUE(range.contains(2026y / March / 1d));
}

TEST(DateRangeTest, DayCountIsInclusiveAndRespectsLeapYears)
{
    EXPECT_EQ(DateRange(2026y / January / 1d, 2026y / January / 31d).dayCount(), 31);
    EXPECT_EQ(DateRange(2024y / February / 1d, 2024y / March / 1d).dayCount(), 30);
    EXPECT_EQ(DateRange(2025y / February / 1d, 2025y / March / 1d).dayCount(), 29);
    EXPECT_EQ(DateRange(2024y / January / 1d, 2024y / December / 31d).dayCount(), 366);
}

TEST(DateRangeTest, OfMonthCoversTheWholeMonth)
{
    const DateRange range = DateRange::ofMonth(YearMonth(2024, 2));
    EXPECT_EQ(range.first(), 2024y / February / 1d);
    EXPECT_EQ(range.last(), 2024y / February / 29d);
    EXPECT_EQ(range.dayCount(), 29);
}

TEST(DateRangeTest, ContainsIncludesBothEnds)
{
    const DateRange range(2026y / March / 10d, 2026y / March / 20d);
    EXPECT_TRUE(range.contains(2026y / March / 10d));
    EXPECT_TRUE(range.contains(2026y / March / 20d));
    EXPECT_FALSE(range.contains(2026y / March / 9d));
    EXPECT_FALSE(range.contains(2026y / March / 21d));
}

TEST(DateRangeTest, AdjacentRangesDoNotOverlapButTouchingOnesDo)
{
    const DateRange first(2026y / March / 1d, 2026y / March / 10d);
    const DateRange adjacent(2026y / March / 11d, 2026y / March / 20d);
    const DateRange touching(2026y / March / 10d, 2026y / March / 20d);
    EXPECT_FALSE(first.overlaps(adjacent));
    EXPECT_FALSE(adjacent.overlaps(first));
    EXPECT_TRUE(first.overlaps(touching));
    EXPECT_TRUE(touching.overlaps(first));
}

TEST(DateRangeTest, IntersectionIsTheSharedPart)
{
    const DateRange first(2026y / March / 1d, 2026y / March / 20d);
    const DateRange second(2026y / March / 10d, 2026y / April / 5d);
    const auto shared = first.intersection(second);
    ASSERT_TRUE(shared.has_value());
    EXPECT_EQ(*shared, DateRange(2026y / March / 10d, 2026y / March / 20d));
    EXPECT_EQ(second.intersection(first), shared);
}

TEST(DateRangeTest, IntersectionOfDisjointRangesIsEmpty)
{
    const DateRange first(2026y / March / 1d, 2026y / March / 9d);
    const DateRange second(2026y / March / 10d, 2026y / March / 20d);
    EXPECT_FALSE(first.intersection(second).has_value());
}

TEST(DateRangeTest, MonthsIncludePartiallyCoveredMonths)
{
    const DateRange range(2025y / November / 30d, 2026y / February / 1d);
    std::vector<YearMonth> months;
    for (const YearMonth month : range.months())
        months.push_back(month);

    const std::vector expected{YearMonth(2025, 11), YearMonth(2025, 12), YearMonth(2026, 1), YearMonth(2026, 2)};
    EXPECT_EQ(months, expected);
}
