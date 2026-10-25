#include "stapik/storage/Quarantine.hpp"

#include "support/FakeClock.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::storage::quarantineFile;

    void writeText(const fs::path& path, const std::string& text)
    {
        std::ofstream file(path);
        file << text;
    }

    std::string readText(const fs::path& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    class QuarantineTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-quarantine");
            m_file = m_directory / "data.json";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
        fs::path m_file;
        stapik::test::FakeClock m_clock;
    };
}

TEST_F(QuarantineTest, FileIsMovedToANameWithTheTimestamp)
{
    writeText(m_file, "broken");

    const auto result = quarantineFile(m_file, m_clock.now());

    ASSERT_TRUE(result);
    EXPECT_EQ(result->filename().string(), "data.json.corrupt-20260115-103000");
    EXPECT_FALSE(fs::exists(m_file));
    EXPECT_EQ(readText(*result), "broken");
}

TEST_F(QuarantineTest, SecondFileInTheSameSecondDoesNotOverwriteTheFirst)
{
    writeText(m_file, "first");
    const auto first = quarantineFile(m_file, m_clock.now());
    writeText(m_file, "second");

    const auto second = quarantineFile(m_file, m_clock.now());

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(*first, *second);
    EXPECT_EQ(second->filename().string(), "data.json.corrupt-20260115-103000-2");
    EXPECT_EQ(readText(*first), "first");
    EXPECT_EQ(readText(*second), "second");
}

TEST_F(QuarantineTest, MissingFileIsReportedAsAFailure)
{
    const auto result = quarantineFile(m_file, m_clock.now());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().path, m_file);
    EXPECT_EQ(result.error().step, "move aside");
}
