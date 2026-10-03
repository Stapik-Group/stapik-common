#include "stapik/sync/Timestamp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>

namespace
{
    using namespace std::chrono;
    using stapik::sync::parseIso8601;
    using stapik::sync::toIso8601;
    using stapik::sync::TimestampPrecision;
}

TEST(TimestampTest, ParsesFullMicrosecondTimestamp)
{
    const auto parsed = parseIso8601("2026-09-30T12:34:56.123456Z");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(toIso8601(*parsed, TimestampPrecision::Microseconds), "2026-09-30T12:34:56.123456Z");
}

TEST(TimestampTest, RoundTripsAtMicrosecondPrecision)
{
    const auto original = floor<microseconds>(system_clock::now());

    const auto text = toIso8601(original, TimestampPrecision::Microseconds);
    const auto parsed = parseIso8601(text);

    ASSERT_TRUE(parsed.has_value()) << text;
    EXPECT_EQ(floor<microseconds>(*parsed), original) << text;
}

TEST(TimestampTest, FormatsSecondsByDefaultAndDropsFraction)
{
    const auto parsed = parseIso8601("2026-09-30T12:34:56.999999Z");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(toIso8601(*parsed), "2026-09-30T12:34:56Z");
}

TEST(TimestampTest, ShortFractionIsPaddedToMicroseconds)
{
    const auto shortFraction = parseIso8601("2026-09-30T12:34:56.1Z");
    const auto paddedFraction = parseIso8601("2026-09-30T12:34:56.100000Z");

    ASSERT_TRUE(shortFraction.has_value());
    ASSERT_TRUE(paddedFraction.has_value());
    EXPECT_EQ(*shortFraction, *paddedFraction);
}

TEST(TimestampTest, NanosecondFractionIsTruncatedToMicroseconds)
{
    const auto nanoseconds = parseIso8601("2026-09-30T12:34:56.123456789Z");
    const auto microseconds = parseIso8601("2026-09-30T12:34:56.123456Z");

    ASSERT_TRUE(nanoseconds.has_value());
    ASSERT_TRUE(microseconds.has_value());
    EXPECT_EQ(*nanoseconds, *microseconds);
}

TEST(TimestampTest, AppliesUtcOffset)
{
    const auto withOffset = parseIso8601("2026-09-30T14:34:56+02:00");
    const auto negativeOffset = parseIso8601("2026-09-30T07:34:56-05:00");
    const auto utc = parseIso8601("2026-09-30T12:34:56Z");

    ASSERT_TRUE(withOffset.has_value());
    ASSERT_TRUE(negativeOffset.has_value());
    ASSERT_TRUE(utc.has_value());
    EXPECT_EQ(*withOffset, *utc);
    EXPECT_EQ(*negativeOffset, *utc);
}

TEST(TimestampTest, MissingSuffixIsTreatedAsUtc)
{
    const auto withoutSuffix = parseIso8601("2026-09-30T12:34:56");
    const auto withSuffix = parseIso8601("2026-09-30T12:34:56Z");

    ASSERT_TRUE(withoutSuffix.has_value());
    ASSERT_TRUE(withSuffix.has_value());
    EXPECT_EQ(*withoutSuffix, *withSuffix);
}

TEST(TimestampTest, ParsesUnixEpoch)
{
    const auto parsed = parseIso8601("1970-01-01T00:00:00Z");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, system_clock::time_point{});
}

TEST(TimestampTest, RejectsInvalidInput)
{
    const std::array invalidInputs = {
        "",
        "garbage",
        "2026-09-30",
        "2026-09-30T12:34Z",
        "2026-13-30T12:34:56Z",
        "2026-02-30T12:34:56Z",
        "2026-09-30T24:00:00Z",
        "2026-09-30T12:60:00Z",
        "2026-09-30T12:34:60Z",
        "2026-09-30T12:34:56.Z",
        "2026-09-30T12:34:56.1234567890Z",
        "2026-09-30T12:34:56Zx",
        "2026-09-30T12:34:56+0200",
        "2026-09-30T12:34:56+24:00",
        "2026/09/30T12:34:56Z",
        "26-09-30T12:34:56Z",
    };

    for (const auto* input : invalidInputs)
        EXPECT_FALSE(parseIso8601(input).has_value()) << input;
}

TEST(TimestampTest, LegacyFromIso8601ReturnsEpochOnError)
{
    EXPECT_EQ(stapik::sync::fromIso8601("not a timestamp"), system_clock::time_point{});
}

TEST(TimestampTest, LegacyFromIso8601ParsesValidInput)
{
    const auto expected = parseIso8601("2026-09-30T12:34:56Z");

    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(stapik::sync::fromIso8601("2026-09-30T12:34:56Z"), *expected);
}
