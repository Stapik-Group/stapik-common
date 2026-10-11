#include "stapik/log/RingBufferSink.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    using stapik::log::Level;
    using stapik::log::LogRecord;
    using stapik::log::RingBufferSink;

    LogRecord recordWith(const std::string& message)
    {
        return LogRecord{.level = Level::Info, .message = message, .timestamp = {}};
    }
}

TEST(RingBufferSinkTest, KeepsRecordsOldestFirst)
{
    RingBufferSink sink(3);

    sink.write(recordWith("one"));
    sink.write(recordWith("two"));

    const auto records = sink.snapshot();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].message, "one");
    EXPECT_EQ(records[1].message, "two");
}

TEST(RingBufferSinkTest, DropsTheOldestRecordWhenFull)
{
    RingBufferSink sink(2);

    sink.write(recordWith("one"));
    sink.write(recordWith("two"));
    sink.write(recordWith("three"));

    const auto records = sink.snapshot();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].message, "two");
    EXPECT_EQ(records[1].message, "three");
}

TEST(RingBufferSinkTest, ClearRemovesEverything)
{
    RingBufferSink sink(2);
    sink.write(recordWith("one"));

    sink.clear();

    EXPECT_TRUE(sink.snapshot().empty());
}

TEST(RingBufferSinkTest, ZeroCapacityKeepsNothing)
{
    RingBufferSink sink(0);

    sink.write(recordWith("one"));

    EXPECT_TRUE(sink.snapshot().empty());
}
