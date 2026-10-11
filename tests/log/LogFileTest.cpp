#include "stapik/log/LogFile.hpp"

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
    using stapik::log::rotateLogFiles;

    constexpr FileRotation THREE_FILES_OF_TEN_BYTES{10, 3};

    class LogFileTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-logfile-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
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

        static void write(const fs::path& file, const std::string& content)
        {
            std::ofstream(file, std::ios::binary) << content;
        }

        static std::string read(const fs::path& file)
        {
            std::ifstream stream(file, std::ios::binary);
            std::stringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        fs::path m_directory;
        fs::path m_logFile;
    };
}

TEST_F(LogFileTest, MissingFileIsLeftAlone)
{
    rotateLogFiles(m_logFile, THREE_FILES_OF_TEN_BYTES);

    EXPECT_FALSE(fs::exists(m_logFile));
    EXPECT_FALSE(fs::exists(backup(1)));
}

TEST_F(LogFileTest, FileBelowMaximumSizeIsNotRotated)
{
    write(m_logFile, "123456789");

    rotateLogFiles(m_logFile, THREE_FILES_OF_TEN_BYTES);

    EXPECT_EQ(read(m_logFile), "123456789");
    EXPECT_FALSE(fs::exists(backup(1)));
}

TEST_F(LogFileTest, FileAtMaximumSizeBecomesTheFirstBackup)
{
    write(m_logFile, "0123456789");

    rotateLogFiles(m_logFile, THREE_FILES_OF_TEN_BYTES);

    EXPECT_FALSE(fs::exists(m_logFile));
    EXPECT_EQ(read(backup(1)), "0123456789");
}

TEST_F(LogFileTest, BackupsAreShiftedAndTheOldestIsDropped)
{
    write(m_logFile, "current....");
    write(backup(1), "newest.....");
    write(backup(2), "oldest.....");

    rotateLogFiles(m_logFile, THREE_FILES_OF_TEN_BYTES);

    EXPECT_FALSE(fs::exists(m_logFile));
    EXPECT_EQ(read(backup(1)), "current....");
    EXPECT_EQ(read(backup(2)), "newest.....");
    EXPECT_FALSE(fs::exists(backup(3)));
}

TEST_F(LogFileTest, SingleFileRotationDiscardsTheFullFile)
{
    write(m_logFile, "0123456789");

    rotateLogFiles(m_logFile, FileRotation{10, 1});

    EXPECT_FALSE(fs::exists(m_logFile));
    EXPECT_FALSE(fs::exists(backup(1)));
}
