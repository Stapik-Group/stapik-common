#include "stapik/domain/YearMonthRange.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using namespace stapik::domain;

namespace
{
    std::vector<YearMonth> collect(const YearMonthRange& range)
    {
        std::vector<YearMonth> months;
        for (const YearMonth month : range)
            months.push_back(month);
        return months;
    }
}

TEST(YearMonthRangeTest, SingleMonthRangeYieldsThatMonth)
{
    const YearMonthRange range(YearMonth(2026, 4), YearMonth(2026, 4));
    EXPECT_EQ(range.size(), 1);
    EXPECT_EQ(collect(range), std::vector{YearMonth(2026, 4)});
}

TEST(YearMonthRangeTest, IterationIsInclusiveAndChronological)
{
    const YearMonthRange range(YearMonth(2026, 2), YearMonth(2026, 5));
    const std::vector expected{YearMonth(2026, 2), YearMonth(2026, 3), YearMonth(2026, 4), YearMonth(2026, 5)};
    EXPECT_EQ(collect(range), expected);
    EXPECT_EQ(range.size(), 4);
}

TEST(YearMonthRangeTest, IterationCrossesYearBoundaries)
{
    const YearMonthRange range(YearMonth(2025, 11), YearMonth(2026, 2));
    const std::vector expected{YearMonth(2025, 11), YearMonth(2025, 12), YearMonth(2026, 1), YearMonth(2026, 2)};
    EXPECT_EQ(collect(range), expected);
}

TEST(YearMonthRangeTest, FullYearHasTwelveMonths)
{
    const YearMonthRange range(YearMonth(2026, 1), YearMonth(2026, 12));
    EXPECT_EQ(range.size(), 12);
    EXPECT_EQ(collect(range).size(), 12U);
}

TEST(YearMonthRangeTest, IterationReachesTheLastSupportedMonth)
{
    const YearMonthRange range(YearMonth(9999, 11), YearMonth(9999, 12));
    const std::vector expected{YearMonth(9999, 11), YearMonth(9999, 12)};
    EXPECT_EQ(collect(range), expected);
}

TEST(YearMonthRangeTest, ReversedRangeIsRejected)
{
    EXPECT_THROW(YearMonthRange(YearMonth(2026, 5), YearMonth(2026, 4)), std::invalid_argument);
}

TEST(YearMonthRangeTest, ContainsIncludesBothEnds)
{
    const YearMonthRange range(YearMonth(2026, 2), YearMonth(2026, 5));
    EXPECT_TRUE(range.contains(YearMonth(2026, 2)));
    EXPECT_TRUE(range.contains(YearMonth(2026, 5)));
    EXPECT_TRUE(range.contains(YearMonth(2026, 3)));
    EXPECT_FALSE(range.contains(YearMonth(2026, 1)));
    EXPECT_FALSE(range.contains(YearMonth(2026, 6)));
}

TEST(YearMonthRangeTest, PostIncrementReturnsPreviousPosition)
{
    const YearMonthRange range(YearMonth(2026, 1), YearMonth(2026, 3));
    auto iterator = range.begin();
    const auto previous = iterator++;
    EXPECT_EQ(*previous, YearMonth(2026, 1));
    EXPECT_EQ(*iterator, YearMonth(2026, 2));
}
