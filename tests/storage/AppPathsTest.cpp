#include "stapik/storage/AppPaths.hpp"

#include "support/Environment.hpp"
#include "support/ScopedEnvironment.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace
{
    namespace fs = std::filesystem;

#ifdef _WIN32
    constexpr auto USER_PROFILE_VARIABLE = "APPDATA";
#else
    constexpr auto USER_PROFILE_VARIABLE = "HOME";
#endif

    class AppPathsTest : public testing::Test
    {
    protected:
        stapik::test::ScopedEnvironment m_appData{ "APPDATA" };
        stapik::test::ScopedEnvironment m_localAppData{ "LOCALAPPDATA" };
        stapik::test::ScopedEnvironment m_home{ "HOME" };
        stapik::test::ScopedEnvironment m_dataHome{ "XDG_DATA_HOME" };
        stapik::test::ScopedEnvironment m_configHome{ "XDG_CONFIG_HOME" };
        stapik::test::ScopedEnvironment m_cacheHome{ "XDG_CACHE_HOME" };
    };
}

TEST_F(AppPathsTest, UsesXdgVariablesWhenSet)
{
    stapik::test::setEnvironment("XDG_DATA_HOME", "/xdg/data");
    stapik::test::setEnvironment("XDG_CONFIG_HOME", "/xdg/config");
    stapik::test::setEnvironment("XDG_CACHE_HOME", "/xdg/cache");

    EXPECT_EQ(AppPaths::userDataDir("app"), "/xdg/data/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/xdg/config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/xdg/cache/app");
}

#ifdef _WIN32
TEST_F(AppPathsTest, FallsBackToAppDataWhenXdgVariableIsUnset)
{
    const fs::path roaming = "C:\\Users\\user\\AppData\\Roaming";
    const fs::path local = "C:\\Users\\user\\AppData\\Local";
    stapik::test::setEnvironment("APPDATA", roaming.string());
    stapik::test::setEnvironment("LOCALAPPDATA", local.string());
    stapik::test::unsetEnvironment("XDG_DATA_HOME");
    stapik::test::unsetEnvironment("XDG_CONFIG_HOME");
    stapik::test::unsetEnvironment("XDG_CACHE_HOME");

    EXPECT_EQ(AppPaths::userDataDir("app"), roaming / "app" / "data");
    EXPECT_EQ(AppPaths::userConfigDir("app"), roaming / "app" / "config");
    EXPECT_EQ(AppPaths::userCacheDir("app"), local / "app" / "cache");
}
#else
TEST_F(AppPathsTest, FallsBackToHomeWhenXdgVariableIsUnset)
{
    stapik::test::setEnvironment("HOME", "/home/user");
    stapik::test::unsetEnvironment("XDG_DATA_HOME");
    stapik::test::unsetEnvironment("XDG_CONFIG_HOME");
    stapik::test::unsetEnvironment("XDG_CACHE_HOME");

    EXPECT_EQ(AppPaths::userDataDir("app"), "/home/user/.local/share/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/home/user/.config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/home/user/.cache/app");
}

TEST_F(AppPathsTest, EmptyXdgVariableIsTreatedAsUnset)
{
    stapik::test::setEnvironment("HOME", "/home/user");
    stapik::test::setEnvironment("XDG_DATA_HOME", "");
    stapik::test::setEnvironment("XDG_CONFIG_HOME", "");
    stapik::test::setEnvironment("XDG_CACHE_HOME", "");

    EXPECT_EQ(AppPaths::userDataDir("app"), "/home/user/.local/share/app");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/home/user/.config/app");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/home/user/.cache/app");
}
#endif

TEST_F(AppPathsTest, FallsBackToTempDirectoryWithoutHome)
{
    stapik::test::unsetEnvironment(USER_PROFILE_VARIABLE);
    stapik::test::unsetEnvironment("XDG_DATA_HOME");

    EXPECT_EQ(AppPaths::userDataDir("app"), fs::temp_directory_path() / "app");
}

TEST_F(AppPathsTest, EnsureUserDataDirCreatesTheDirectory)
{
    const fs::path root = stapik::test::createTemporaryDirectory("stapik-apppaths");

    stapik::test::setEnvironment("XDG_DATA_HOME", root.string());

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
            m_root = stapik::test::createTemporaryDirectory("stapik-common-res");
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

TEST_F(AppPathsTest, RootOverrideRelocatesAllUserDirectories)
{
    stapik::test::setEnvironment("XDG_DATA_HOME", "/xdg/data");
    stapik::test::setEnvironment("XDG_CONFIG_HOME", "/xdg/config");
    stapik::test::setEnvironment("XDG_CACHE_HOME", "/xdg/cache");
    AppPaths::setRootOverride("/portable/root");

    EXPECT_EQ(AppPaths::userDataDir("app"), "/portable/root/data");
    EXPECT_EQ(AppPaths::userConfigDir("app"), "/portable/root/config");
    EXPECT_EQ(AppPaths::userCacheDir("app"), "/portable/root/cache");

    AppPaths::clearRootOverride();
}

TEST_F(AppPathsTest, ClearingRootOverrideRestoresDefaults)
{
    stapik::test::setEnvironment("XDG_DATA_HOME", "/xdg/data");
    AppPaths::setRootOverride("/portable/root");
    AppPaths::clearRootOverride();

    EXPECT_FALSE(AppPaths::rootOverride().has_value());
    EXPECT_EQ(AppPaths::userDataDir("app"), "/xdg/data/app");
}

TEST(AppPathsPortableTest, PortableRootLivesNextToTheExecutable)
{
    EXPECT_EQ(AppPaths::portableRoot(), AppPaths::executableDir() / "portable");
}

TEST_F(CommonResourcesDirTest, ResourcesDirPrefersTheDirectoryNextToTheExecutable)
{
    fs::create_directories(m_executableDir / "resources");
    fs::create_directories(m_root / "app" / "resources");

    EXPECT_EQ(AppPaths::resolveResourcesDir(m_executableDir), m_executableDir / "resources");
}

TEST_F(CommonResourcesDirTest, ResourcesDirFallsBackToTheParentOfTheExecutableDirectory)
{
    EXPECT_EQ(AppPaths::resolveResourcesDir(m_executableDir), m_root / "app" / "resources");
}

TEST_F(CommonResourcesDirTest, ResourcesDirDoesNotThrowWhenTheExecutableDirectoryCannotBeSearched)
{
#ifdef _WIN32
    GTEST_SKIP() << "Windows does not refuse a directory through its permissions";
#endif
    fs::permissions(m_executableDir, fs::perms::none);

    fs::path resolved;
    EXPECT_NO_THROW(resolved = AppPaths::resolveResourcesDir(m_executableDir));

    fs::permissions(m_executableDir, fs::perms::owner_all);
    EXPECT_EQ(resolved, m_root / "app" / "resources");
}
