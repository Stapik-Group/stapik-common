#include "stapik/time/Clock.hpp"

#include "support/FakeClock.hpp"

#include <gtest/gtest.h>

#include <chrono>

using namespace std::chrono_literals;
using stapik::test::FakeClock;
using stapik::time::SystemClock;
using stapik::time::systemClock;

TEST(ClockTest, SystemClockReportsTheCurrentTime)
{
    const auto before = std::chrono::system_clock::now();
    const auto reported = SystemClock().now();
    const auto after = std::chrono::system_clock::now();

    EXPECT_GE(reported, before);
    EXPECT_LE(reported, after);
}

TEST(ClockTest, SystemClockIsShared)
{
    EXPECT_EQ(&systemClock(), &systemClock());
}

TEST(ClockTest, FakeClockStandsStillUntilMoved)
{
    FakeClock clock;
    const auto start = clock.now();

    EXPECT_EQ(start, FakeClock::DEFAULT_START);
    EXPECT_EQ(clock.now(), start);

    clock.advance(90min);
    EXPECT_EQ(clock.now(), start + 90min);

    clock.set(start);
    EXPECT_EQ(clock.now(), start);
}
