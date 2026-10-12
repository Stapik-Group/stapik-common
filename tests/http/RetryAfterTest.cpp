#include "stapik/http/RetryAfter.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace std::chrono_literals;
    using namespace stapik::http;

    const stapik::time::TimePoint NOW = std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::October / 12 } + 10h;
}

TEST(RetryAfterTest, NumberIsADelayInSeconds)
{
    EXPECT_EQ(parseRetryAfter("120", NOW), 120s);
    EXPECT_EQ(parseRetryAfter("0", NOW), 0s);
}

TEST(RetryAfterTest, SurroundingWhitespaceIsIgnored)
{
    EXPECT_EQ(parseRetryAfter(" 7 \r\n", NOW), 7s);
}

TEST(RetryAfterTest, HttpDateIsTurnedIntoTheDelayFromNow)
{
    EXPECT_EQ(parseRetryAfter("Mon, 12 Oct 2026 10:01:30 GMT", NOW), 90s);
}

TEST(RetryAfterTest, DateInThePastMeansNoWaiting)
{
    EXPECT_EQ(parseRetryAfter("Mon, 12 Oct 2026 09:00:00 GMT", NOW), 0s);
}

TEST(RetryAfterTest, NonsenseHasNoValue)
{
    EXPECT_FALSE(parseRetryAfter("", NOW).has_value());
    EXPECT_FALSE(parseRetryAfter("soon", NOW).has_value());
    EXPECT_FALSE(parseRetryAfter("-5", NOW).has_value());
    EXPECT_FALSE(parseRetryAfter("12 seconds", NOW).has_value());
    EXPECT_FALSE(parseRetryAfter("Mon, 32 Oct 2026 10:00:00 GMT", NOW).has_value());
    EXPECT_FALSE(parseRetryAfter("Mon, 12 Foo 2026 10:00:00 GMT", NOW).has_value());
}
