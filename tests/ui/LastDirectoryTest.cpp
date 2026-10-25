#include "stapik/ui/dialog/LastDirectory.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::SettingsStore;
    using stapik::ui::loadLastDirectory;
    using stapik::ui::saveLastDirectory;

    constexpr auto KEY = "dialog.test.lastDirectory";

    class LastDirectoryTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-lastdirectory");
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        [[nodiscard]] SettingsStore makeStore() const
        {
            return SettingsStore(m_directory / "settings.json");
        }

        fs::path m_directory;
    };
}

TEST_F(LastDirectoryTest, NothingStoredYieldsNoDirectory)
{
    EXPECT_FALSE(loadLastDirectory(makeStore(), KEY).has_value());
}

TEST_F(LastDirectoryTest, ChosenFileRemembersItsDirectory)
{
    auto store = makeStore();
    const auto file = m_directory / "report.csv";
    std::ofstream(file).put('x');

    ASSERT_TRUE(saveLastDirectory(store, KEY, file));

    EXPECT_EQ(loadLastDirectory(store, KEY), m_directory);
}

TEST_F(LastDirectoryTest, FileThatDoesNotExistYetRemembersItsDirectory)
{
    auto store = makeStore();

    ASSERT_TRUE(saveLastDirectory(store, KEY, m_directory / "new.csv"));

    EXPECT_EQ(loadLastDirectory(store, KEY), m_directory);
}

TEST_F(LastDirectoryTest, ChosenFolderIsRememberedItself)
{
    auto store = makeStore();
    const auto folder = m_directory / "exports";
    fs::create_directory(folder);

    ASSERT_TRUE(saveLastDirectory(store, KEY, folder));

    EXPECT_EQ(loadLastDirectory(store, KEY), folder);
}

TEST_F(LastDirectoryTest, DirectoryThatNoLongerExistsIsIgnored)
{
    auto store = makeStore();
    const auto folder = m_directory / "gone";
    fs::create_directory(folder);
    ASSERT_TRUE(saveLastDirectory(store, KEY, folder));

    fs::remove(folder);

    EXPECT_FALSE(loadLastDirectory(store, KEY).has_value());
}

TEST_F(LastDirectoryTest, SurvivesReloadingTheStore)
{
    {
        auto store = makeStore();
        ASSERT_TRUE(saveLastDirectory(store, KEY, m_directory / "new.csv"));
    }

    auto reloaded = makeStore();
    reloaded.load();

    EXPECT_EQ(loadLastDirectory(reloaded, KEY), m_directory);
}
