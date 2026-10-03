#include "stapik/app/AppContext.hpp"

#include "support/ScopedEnvironment.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::app::AppContext;
    using stapik::app::AppInfo;

    AppInfo makeInfo(const std::string& internalName, const std::string& displayName = "")
    {
        AppInfo info;
        info.internalName = internalName;
        info.displayName = displayName;
        return info;
    }

    class AppContextTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            AppContext::resetForTests();

            auto pattern = (fs::temp_directory_path() / "stapik-appcontext-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
            setenv("XDG_CONFIG_HOME", (m_directory / "config").c_str(), 1);
        }

        void TearDown() override
        {
            AppContext::resetForTests();

            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;

    private:
        stapik::test::ScopedEnvironment m_configHome{ "XDG_CONFIG_HOME" };
    };
}

TEST_F(AppContextTest, IsNotInitializedByDefault)
{
    EXPECT_FALSE(AppContext::isInitialized());
}

TEST_F(AppContextTest, InitializeStoresAppInfo)
{
    AppInfo info = makeInfo("stapikexample", "Stapik Example");
    info.version = "1.2.3";

    EXPECT_TRUE(AppContext::initialize(info));

    EXPECT_TRUE(AppContext::isInitialized());
    EXPECT_EQ(AppContext::instance().info().internalName, "stapikexample");
    EXPECT_EQ(AppContext::instance().info().displayName, "Stapik Example");
    EXPECT_EQ(AppContext::instance().info().version, "1.2.3");
}

TEST_F(AppContextTest, SecondInitializeIsIgnored)
{
    ASSERT_TRUE(AppContext::initialize(makeInfo("first", "First")));

    EXPECT_FALSE(AppContext::initialize(makeInfo("second", "Second")));

    EXPECT_EQ(AppContext::instance().info().internalName, "first");
    EXPECT_EQ(AppContext::instance().info().displayName, "First");
}

TEST_F(AppContextTest, SecondInitializeWithSameNameIsAlsoIgnored)
{
    ASSERT_TRUE(AppContext::initialize(makeInfo("app", "Original")));

    EXPECT_FALSE(AppContext::initialize(makeInfo("app", "Changed")));

    EXPECT_EQ(AppContext::instance().info().displayName, "Original");
}

TEST_F(AppContextTest, ResetAllowsInitializingAgain)
{
    ASSERT_TRUE(AppContext::initialize(makeInfo("first")));
    AppContext::resetForTests();

    EXPECT_FALSE(AppContext::isInitialized());
    EXPECT_TRUE(AppContext::initialize(makeInfo("second")));
    EXPECT_EQ(AppContext::instance().info().internalName, "second");
}

TEST_F(AppContextTest, LegacyNameInitializesWhenNoContextExists)
{
    AppContext::initializeFromLegacyName("legacyapp");

    EXPECT_TRUE(AppContext::isInitialized());
    EXPECT_EQ(AppContext::instance().info().internalName, "legacyapp");
}

TEST_F(AppContextTest, LegacyNameDoesNotChangeExistingContext)
{
    ASSERT_TRUE(AppContext::initialize(makeInfo("real", "Real")));

    AppContext::initializeFromLegacyName("other");

    EXPECT_EQ(AppContext::instance().info().internalName, "real");
    EXPECT_EQ(AppContext::instance().info().displayName, "Real");
}

TEST_F(AppContextTest, FullInitializeReplacesContextCreatedFromLegacyName)
{
    AppContext::initializeFromLegacyName("app");

    EXPECT_TRUE(AppContext::initialize(makeInfo("app", "Full Info")));

    EXPECT_EQ(AppContext::instance().info().displayName, "Full Info");
}

TEST_F(AppContextTest, FullInitializeWithAnotherNameDoesNotReplaceLegacyContext)
{
    AppContext::initializeFromLegacyName("app");

    EXPECT_FALSE(AppContext::initialize(makeInfo("different", "Different")));

    EXPECT_EQ(AppContext::instance().info().internalName, "app");
}

TEST_F(AppContextTest, LegacyContextIsReplacedOnlyOnce)
{
    AppContext::initializeFromLegacyName("app");
    ASSERT_TRUE(AppContext::initialize(makeInfo("app", "First full")));

    EXPECT_FALSE(AppContext::initialize(makeInfo("app", "Second full")));

    EXPECT_EQ(AppContext::instance().info().displayName, "First full");
}

TEST_F(AppContextTest, SettingsStoreBelongsToTheApplication)
{
    ASSERT_TRUE(AppContext::initialize(makeInfo("context-test-app")));

    EXPECT_EQ(AppContext::instance().settings().filePath(),
        m_directory / "config" / "context-test-app" / "settings.json");
}
