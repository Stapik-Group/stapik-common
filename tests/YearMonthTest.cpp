#include "stapik/domain/YearMonth.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace stapik::domain;

TEST(YearMonthTest, ConstructorRejectsInvalidValues)
{
    EXPECT_THROW(YearMonth(2026, 0), std::invalid_argument);
    EXPECT_THROW(YearMonth(2026, 13), std::invalid_argument);
    EXPECT_THROW(YearMonth(0, 1), std::invalid_argument);
    EXPECT_THROW(YearMonth(10000, 1), std::invalid_argument);
    EXPECT_FALSE(YearMonth::tryCreate(2026, 13).has_value());
    EXPECT_TRUE(YearMonth::tryCreate(2026, 12).has_value());
}

TEST(YearMonthTest, KeyRoundTrips)
{
    const YearMonth value(2026, 3);
    EXPECT_EQ(value.toKey(), "2026-03");
    EXPECT_EQ(YearMonth::fromKey("2026-03"), value);
    EXPECT_EQ(YearMonth(5, 11).toKey(), "0005-11");
    EXPECT_EQ(YearMonth::fromKey("0005-11"), YearMonth(5, 11));
}

TEST(YearMonthTest, FromKeyRejectsMalformedInput)
{
    EXPECT_FALSE(YearMonth::fromKey("").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026-3").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026/03").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026-13").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026-00").has_value());
    EXPECT_FALSE(YearMonth::fromKey("0000-01").has_value());
    EXPECT_FALSE(YearMonth::fromKey("20x6-03").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026-03 ").has_value());
    EXPECT_FALSE(YearMonth::fromKey("+026-03").has_value());
    EXPECT_FALSE(YearMonth::fromKey("2026--3").has_value());
}

TEST(YearMonthTest, OrderingIsChronological)
{
    EXPECT_LT(YearMonth(2025, 12), YearMonth(2026, 1));
    EXPECT_LT(YearMonth(2026, 1), YearMonth(2026, 2));
    EXPECT_EQ(YearMonth(2026, 2), YearMonth(2026, 2));
    EXPECT_GT(YearMonth(2027, 1), YearMonth(2026, 12));
}

TEST(YearMonthTest, KeyOrderMatchesChronologicalOrder)
{
    EXPECT_LT(YearMonth(2025, 12).toKey(), YearMonth(2026, 1).toKey());
    EXPECT_LT(YearMonth(999, 12).toKey(), YearMonth(1000, 1).toKey());
}

TEST(YearMonthTest, NextAndPreviousCrossYearBoundaries)
{
    EXPECT_EQ(YearMonth(2026, 12).next(), YearMonth(2027, 1));
    EXPECT_EQ(YearMonth(2026, 1).previous(), YearMonth(2025, 12));
    EXPECT_EQ(YearMonth(2026, 5).next(), YearMonth(2026, 6));
}

TEST(YearMonthTest, AddMonthsHandlesLargeAndNegativeOffsets)
{
    EXPECT_EQ(YearMonth(2026, 1).addMonths(0), YearMonth(2026, 1));
    EXPECT_EQ(YearMonth(2026, 1).addMonths(25), YearMonth(2028, 2));
    EXPECT_EQ(YearMonth(2026, 3).addMonths(-3), YearMonth(2025, 12));
    EXPECT_EQ(YearMonth(2026, 3).addMonths(-27), YearMonth(2023, 12));
}

TEST(YearMonthTest, AddMonthsThrowsOutsideSupportedRange)
{
    EXPECT_THROW((void)YearMonth(1, 1).previous(), std::out_of_range);
    EXPECT_THROW((void)YearMonth(9999, 12).next(), std::out_of_range);
    EXPECT_THROW((void)YearMonth(2026, 1).addMonths(1000000), std::out_of_range);
}

TEST(YearMonthTest, MonthsUntilIsSigned)
{
    EXPECT_EQ(YearMonth(2026, 1).monthsUntil(YearMonth(2026, 1)), 0);
    EXPECT_EQ(YearMonth(2025, 11).monthsUntil(YearMonth(2026, 2)), 3);
    EXPECT_EQ(YearMonth(2026, 2).monthsUntil(YearMonth(2025, 11)), -3);
}

TEST(YearMonthTest, DaysInMonthRespectsLeapYears)
{
    EXPECT_EQ(YearMonth(2024, 2).daysInMonth(), 29);
    EXPECT_EQ(YearMonth(2025, 2).daysInMonth(), 28);
    EXPECT_EQ(YearMonth(1900, 2).daysInMonth(), 28);
    EXPECT_EQ(YearMonth(2000, 2).daysInMonth(), 29);
    EXPECT_EQ(YearMonth(2026, 4).daysInMonth(), 30);
    EXPECT_EQ(YearMonth(2026, 12).daysInMonth(), 31);
}

TEST(YearMonthTest, FirstAndLastDay)
{
    using namespace std::chrono;
    EXPECT_EQ(YearMonth(2024, 2).firstDay(), 2024y / February / 1d);
    EXPECT_EQ(YearMonth(2024, 2).lastDay(), 2024y / February / 29d);
}

TEST(YearMonthTest, FromDateDropsTheDay)
{
    using namespace std::chrono;
    EXPECT_EQ(YearMonth::fromDate(2026y / October / 3d), YearMonth(2026, 10));
}

TEST(YearMonthTest, CurrentIsAValidMonth)
{
    const auto current = YearMonth::current();
    EXPECT_GE(current.year(), 2026);
    EXPECT_GE(current.month(), 1);
    EXPECT_LE(current.month(), 12);
}

TEST(YearMonthTest, MonthNameKeyMatchesCommonLocaleKeys)
{
    EXPECT_EQ(YearMonth(2026, 1).monthNameKey(), "month.january");
    EXPECT_EQ(YearMonth(2026, 5).monthNameKey(), "month.may");
    EXPECT_EQ(YearMonth(2026, 12).monthNameKey(), "month.december");
}
