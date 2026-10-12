#include "stapik/log/LogReport.hpp"
#include "stapik/log/MemoryLog.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using stapik::log::Level;
    using stapik::log::LogRecord;

    std::vector<LogRecord> sampleRecords()
    {
        return {
            {.level = Level::Debug, .message = "debug line", .timestamp = {}},
            {.level = Level::Info, .message = "info line", .timestamp = {}},
            {.level = Level::Warning, .message = "warning line", .timestamp = {}},
            {.level = Level::Error, .message = "error line", .timestamp = {}},
        };
    }
}

TEST(LogReportTest, FilterKeepsTheMinimumLevelAndAbove)
{
    const auto filtered = stapik::log::filterByLevel(sampleRecords(), Level::Warning);

    ASSERT_EQ(filtered.size(), 2u);
    EXPECT_EQ(filtered[0].message, "warning line");
    EXPECT_EQ(filtered[1].message, "error line");
}

TEST(LogReportTest, FilterAtDebugKeepsEverythingInOrder)
{
    const auto filtered = stapik::log::filterByLevel(sampleRecords(), Level::Debug);

    ASSERT_EQ(filtered.size(), 4u);
    EXPECT_EQ(filtered.front().message, "debug line");
    EXPECT_EQ(filtered.back().message, "error line");
}

TEST(LogReportTest, LogTextHasOneLinePerRecordAndNoTrailingNewline)
{
    const auto text = stapik::log::formatLogText(sampleRecords());

    EXPECT_EQ(text, "1970-01-01 00:00:00 [DEBUG] debug line\n"
                    "1970-01-01 00:00:00 [INFO] info line\n"
                    "1970-01-01 00:00:00 [WARN] warning line\n"
                    "1970-01-01 00:00:00 [ERROR] error line");
}

TEST(LogReportTest, LogTextOfNoRecordsIsEmpty)
{
    EXPECT_TRUE(stapik::log::formatLogText({}).empty());
}

TEST(LogReportTest, BugReportStartsWithApplicationAndVersion)
{
    const auto report = stapik::log::formatBugReport({.application = "Stapik Test", .version = "1.2.3"}, Level::Info, sampleRecords());

    EXPECT_TRUE(report.starts_with("Application: Stapik Test 1.2.3\nPlatform: "));
    EXPECT_NE(report.find("Log level: INFO and above\nRecords: 3\n"), std::string::npos);
}

TEST(LogReportTest, BugReportContainsOnlyRecordsFromTheMinimumLevel)
{
    const auto report = stapik::log::formatBugReport({.application = "App", .version = "1"}, Level::Error, sampleRecords());

    EXPECT_NE(report.find("error line"), std::string::npos);
    EXPECT_EQ(report.find("warning line"), std::string::npos);
    EXPECT_EQ(report.find("info line"), std::string::npos);
}

TEST(LogReportTest, InstallMemoryLogReturnsTheSameSinkEveryTime)
{
    const auto first = stapik::log::installMemoryLog(10);
    const auto second = stapik::log::installMemoryLog(20);

    EXPECT_EQ(first, second);
    EXPECT_EQ(stapik::log::memoryLog(), first);
}
