#include "stapik/log/ConsoleSink.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace
{
    using stapik::log::Level;
    using stapik::log::LogRecord;

    // 2026-10-11 12:30:45 UTC
    constexpr std::chrono::system_clock::time_point SAMPLE_TIME{std::chrono::seconds{1791721845}};
}

TEST(FormatRecordTest, PutsTimeLevelAndMessageOnOneLine)
{
    const LogRecord record{.level = Level::Warning, .message = "disk almost full", .timestamp = SAMPLE_TIME};

    EXPECT_EQ(stapik::log::formatRecord(record), "2026-10-11 12:30:45 [WARN] disk almost full");
}

TEST(ConsoleSinkTest, WritesTheFormattedRecordToStderr)
{
    stapik::log::ConsoleSink sink;
    const LogRecord record{.level = Level::Error, .message = "boom", .timestamp = SAMPLE_TIME};

    testing::internal::CaptureStderr();
    sink.write(record);

    EXPECT_EQ(testing::internal::GetCapturedStderr(), "2026-10-11 12:30:45 [ERROR] boom\n");
}
