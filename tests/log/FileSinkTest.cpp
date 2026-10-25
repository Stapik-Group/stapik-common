#include "stapik/log/FileSink.hpp"

#include "support/FakeClock.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::log::FileRotation;
    using stapik::log::FileSink;
    using stapik::log::Level;
    using stapik::log::LogRecord;

    constexpr FileRotation SMALL_FILES{.maxFileSize = 200, .fileCount = 3};

    class FileSinkTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-filesink");
            m_logFile = m_directory / "stapik.log";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        [[nodiscard]] fs::path backup(const unsigned index) const
        {
            return fs::path(m_logFile.string() + "." + std::to_string(index));
        }

        static std::string read(const fs::path& file)
        {
            std::ifstream stream(file, std::ios::binary);
            std::stringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        static LogRecord recordWith(const std::string& message)
        {
            return LogRecord{Level::Info, message, {}};
        }

        fs::path m_directory;
        fs::path m_logFile;
    };
}

TEST_F(FileSinkTest, WritesAStartMarkerAndFormattedRecords)
{
    {
        FileSink sink(m_logFile);
        sink.write(recordWith("hello"));
    }

    const auto content = read(m_logFile);
    EXPECT_TRUE(content.starts_with("---- started "));
    EXPECT_NE(content.find("[INFO] hello\n"), std::string::npos);
}

TEST_F(FileSinkTest, StartMarkerShowsTheTimeOfTheClock)
{
    {
        const stapik::test::FakeClock clock;
        FileSink sink(m_logFile, SMALL_FILES, clock);
    }

    EXPECT_TRUE(read(m_logFile).starts_with("---- started 2026-01-15 10:30:00 UTC ----\n"));
}

TEST_F(FileSinkTest, AppendsToTheFileOfAPreviousRun)
{
    {
        FileSink sink(m_logFile);
        sink.write(recordWith("first run"));
    }
    {
        FileSink sink(m_logFile);
        sink.write(recordWith("second run"));
    }

    const auto content = read(m_logFile);
    EXPECT_NE(content.find("first run"), std::string::npos);
    EXPECT_NE(content.find("second run"), std::string::npos);
}

TEST_F(FileSinkTest, RotatesWhileRunningOnceTheFileIsFull)
{
    FileSink sink(m_logFile, SMALL_FILES);

    for (int index = 0; index < 20; ++index)
        sink.write(recordWith("message number " + std::to_string(index)));

    EXPECT_TRUE(fs::exists(backup(1)));
    EXPECT_TRUE(fs::exists(backup(2)));
    EXPECT_FALSE(fs::exists(backup(3)));
    EXPECT_NE(read(m_logFile).find("message number 19"), std::string::npos);
}

TEST_F(FileSinkTest, UnwritableLocationDoesNotThrow)
{
    FileSink sink(m_directory / "missing" / "stapik.log");

    EXPECT_NO_THROW(sink.write(recordWith("nowhere")));
}
