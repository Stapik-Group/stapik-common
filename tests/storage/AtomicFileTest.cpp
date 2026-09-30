#include "stapik/storage/AtomicFile.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::storage::writeFileAtomically;

    std::string readAll(const fs::path& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::size_t countEntries(const fs::path& directory)
    {
        std::size_t count = 0;
        for (const auto& entry : fs::directory_iterator(directory))
        {
            static_cast<void>(entry);
            ++count;
        }
        return count;
    }

    class AtomicFileTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-atomicfile-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(AtomicFileTest, WritesNewFile)
{
    const auto path = m_directory / "file.txt";

    ASSERT_TRUE(writeFileAtomically(path, "hello"));

    EXPECT_EQ(readAll(path), "hello");
}

TEST_F(AtomicFileTest, CreatesMissingParentDirectories)
{
    const auto path = m_directory / "a" / "b" / "file.txt";

    ASSERT_TRUE(writeFileAtomically(path, "nested"));

    EXPECT_EQ(readAll(path), "nested");
}

TEST_F(AtomicFileTest, OverwritesExistingFileCompletely)
{
    const auto path = m_directory / "file.txt";

    ASSERT_TRUE(writeFileAtomically(path, "a much longer first version"));
    ASSERT_TRUE(writeFileAtomically(path, "short"));

    EXPECT_EQ(readAll(path), "short");
}

TEST_F(AtomicFileTest, LeavesNoTemporaryFilesBehind)
{
    const auto path = m_directory / "file.txt";

    ASSERT_TRUE(writeFileAtomically(path, "one"));
    ASSERT_TRUE(writeFileAtomically(path, "two"));

    EXPECT_EQ(countEntries(m_directory), 1u);
}

TEST_F(AtomicFileTest, AppliesRequestedPermissions)
{
    const auto privatePath = m_directory / "private.txt";
    const auto sharedPath = m_directory / "shared.txt";

    ASSERT_TRUE(writeFileAtomically(privatePath, "secret", stapik::storage::OWNER_READ_WRITE));
    ASSERT_TRUE(writeFileAtomically(sharedPath, "data"));

    EXPECT_EQ(fs::status(privatePath).permissions() & fs::perms::all, stapik::storage::OWNER_READ_WRITE);
    EXPECT_EQ(fs::status(sharedPath).permissions() & fs::perms::all, stapik::storage::DEFAULT_FILE_PERMISSIONS);
}

TEST_F(AtomicFileTest, FailsWhenParentIsNotADirectory)
{
    const auto blocker = m_directory / "blocker";
    ASSERT_TRUE(writeFileAtomically(blocker, "i am a file"));

    EXPECT_FALSE(writeFileAtomically(blocker / "file.txt", "data"));

    EXPECT_EQ(readAll(blocker), "i am a file");
}

TEST_F(AtomicFileTest, FailedRenameKeepsTargetAndRemovesTemporaryFile)
{
    const auto targetDirectory = m_directory / "target";
    fs::create_directory(targetDirectory);

    EXPECT_FALSE(writeFileAtomically(targetDirectory, "data"));

    EXPECT_TRUE(fs::is_directory(targetDirectory));
    EXPECT_EQ(countEntries(m_directory), 1u);
}
