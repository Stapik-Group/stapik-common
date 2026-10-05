#include "stapik/sync/PartitionKey.hpp"

#include <gtest/gtest.h>

#include <optional>

using stapik::sync::partitionKeyForYear;
using stapik::sync::yearFromPartitionKey;

TEST(PartitionKeyTest, YearIsFormattedAsFourDigits)
{
    EXPECT_EQ(partitionKeyForYear(2025), "2025");
    EXPECT_EQ(partitionKeyForYear(99), "0099");
}

TEST(PartitionKeyTest, FourDigitKeyIsParsedBackToYear)
{
    EXPECT_EQ(yearFromPartitionKey("2025"), std::optional<int>{ 2025 });
    EXPECT_EQ(yearFromPartitionKey("0099"), std::optional<int>{ 99 });
}

TEST(PartitionKeyTest, KeysThatAreNotPlainYearsAreRejected)
{
    EXPECT_FALSE(yearFromPartitionKey("").has_value());
    EXPECT_FALSE(yearFromPartitionKey("25").has_value());
    EXPECT_FALSE(yearFromPartitionKey("20255").has_value());
    EXPECT_FALSE(yearFromPartitionKey("abcd").has_value());
    EXPECT_FALSE(yearFromPartitionKey("-025").has_value());
    EXPECT_FALSE(yearFromPartitionKey(" 2025").has_value());
}

TEST(PartitionKeyTest, KeyAndYearRoundTrip)
{
    for (const int year : { 1, 1999, 2026, 9999 })
        EXPECT_EQ(yearFromPartitionKey(partitionKeyForYear(year)), std::optional<int>{ year });
}
