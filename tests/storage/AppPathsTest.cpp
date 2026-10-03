#include "stapik/storage/AppPaths.hpp"

#include "support/ScopedEnvironment.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace
{
    namespace fs = std::filesystem;

    class AppPathsTest : public testing::Test
    {
    protected:
        stapik::test::ScopedEnvironment m_home{ "HOME" };
        stapik::test::ScopedEnvironment m_dataHome{ "XDG_DATA_HOME" };
        stapik::test::ScopedEnvironment m_configHome{ "XDG_CONFIG_HOME" };
        stapik::test::ScopedEnvironment m_cacheHome{ "XDG_CACHE_HOME" };
    };
}

TEST_F(AppPathsTest, UsesXdgVariablesWhenSet)
{
    setenv("XDG_DATA_HOME", "/xdg/data", 1);
    setenv("XDG_CONFIG_HOME", "/xdg/config", 1);
    setenv("XDG_CACHE_HOME", "/xdg/cache", 1);

    EXPECT_EQ(AppPaths::userDataDir("app"), "/xdg/data/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/xdg/config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/xdg/cache/app");
}

TEST_F(AppPathsTest, FallsBackToHomeWhenXdgVariableIsUnset)
{
    setenv("HOME", "/home/user", 1);
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_CACHE_HOME");

    EXPECT_EQ(AppPaths::userDataDir("app"), "/home/user/.local/share/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/home/user/.config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/home/user/.cache/app");
}

TEST_F(AppPathsTest, EmptyXdgVariableIsTreatedAsUnset)
{
    setenv("HOME", "/home/user", 1);
    setenv("XDG_DATA_HOME", "", 1);
    setenv("XDG_CONFIG_HOME", "", 1);
    setenv("XDG_CACHE_HOME", "", 1);

    EXPECT_EQ(AppPaths::userDataDir("app"), "/home/user/.local/share/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/home/user/.config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/home/user/.cache/app");
}

TEST_F(AppPathsTest, FallsBackToTempDirectoryWithoutHome)
{
    unsetenv("HOME");
    unsetenv("XDG_DATA_HOME");

    EXPECT_EQ(AppPaths::userDataDir("app"), fs::temp_directory_path() / "app");
}

TEST_F(AppPathsTest, EnsureUserDataDirCreatesTheDirectory)
{
    auto pattern = (fs::temp_directory_path() / "stapik-apppaths-XXXXXX").string();
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    const fs::path root = pattern;

    setenv("XDG_DATA_HOME", root.c_str(), 1);

    const auto directory = AppPaths::ensureUserDataDir("app");

    EXPECT_EQ(directory, root / "app");
    EXPECT_TRUE(fs::is_directory(directory));

    std::error_code errorCode;
    fs::remove_all(root, errorCode);
}

namespace
{
    class CommonResourcesDirTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-common-res-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_root = pattern;
            m_executableDir = m_root / "app" / "bin";
            fs::create_directories(m_executableDir);
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_root, errorCode);
        }

        fs::path m_root;
        fs::path m_executableDir;
    };
}

TEST_F(CommonResourcesDirTest, PrefersTheCopyNextToTheApplicationResources)
{
    fs::create_directories(m_executableDir / "resources" / "stapik-common");
    fs::create_directories(m_root / "app" / "share" / "stapik-common");
    fs::create_directories(m_root / "source");

    EXPECT_EQ(AppPaths::resolveCommonResourcesDir(m_executableDir, m_root / "source"),
        m_executableDir / "resources" / "stapik-common");
}

TEST_F(CommonResourcesDirTest, UsesTheInstalledShareDirectoryNext)
{
    fs::create_directories(m_root / "app" / "share" / "stapik-common");
    fs::create_directories(m_root / "source");

    EXPECT_EQ(AppPaths::resolveCommonResourcesDir(m_executableDir, m_root / "source"),
        m_root / "app" / "share" / "stapik-common");
}

TEST_F(CommonResourcesDirTest, FallsBackToTheSourceTree)
{
    fs::create_directories(m_root / "source");

    EXPECT_EQ(AppPaths::resolveCommonResourcesDir(m_executableDir, m_root / "source"), m_root / "source");
}

TEST_F(CommonResourcesDirTest, ReturnsTheFirstCandidateWhenNothingExists)
{
    EXPECT_EQ(AppPaths::resolveCommonResourcesDir(m_executableDir, m_root / "missing"),
        m_executableDir / "resources" / "stapik-common");
    EXPECT_EQ(AppPaths::resolveCommonResourcesDir(m_executableDir, {}),
        m_executableDir / "resources" / "stapik-common");
}
