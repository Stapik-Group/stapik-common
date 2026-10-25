#include "stapik/storage/DirectoryLock.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace
{
    namespace fs = std::filesystem;
    using stapik::storage::DirectoryLock;

    class DirectoryLockTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
#ifdef _WIN32
            GTEST_SKIP() << "Windows keeps instances apart with SingleInstanceGuard";
#else
            m_directory = stapik::test::createTemporaryDirectory("stapik-dirlock");
#endif
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(DirectoryLockTest, SecondLockOnTheSameDirectoryIsRefused)
{
    const auto first = DirectoryLock::tryAcquire(m_directory);

    EXPECT_NE(first, nullptr);
    EXPECT_EQ(DirectoryLock::tryAcquire(m_directory), nullptr);
}

TEST_F(DirectoryLockTest, LockIsReleasedWithTheObject)
{
    DirectoryLock::tryAcquire(m_directory).reset();

    EXPECT_NE(DirectoryLock::tryAcquire(m_directory), nullptr);
}

TEST_F(DirectoryLockTest, DifferentDirectoriesDoNotInterfere)
{
    const auto other = m_directory / "other";
    fs::create_directory(other);

    const auto first = DirectoryLock::tryAcquire(m_directory);
    const auto second = DirectoryLock::tryAcquire(other);

    EXPECT_NE(first, nullptr);
    EXPECT_NE(second, nullptr);
}

TEST_F(DirectoryLockTest, MissingDirectoryDoesNotBlockStartup)
{
    EXPECT_NE(DirectoryLock::tryAcquire(m_directory / "missing"), nullptr);
}
