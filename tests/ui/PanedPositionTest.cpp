#include "stapik/ui/window/PanedPosition.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::SettingsStore;
    using stapik::ui::loadPanedPosition;
    using stapik::ui::MAX_PANED_POSITION;
    using stapik::ui::savePanedPosition;

    class PanedPositionTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-panedposition");
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

TEST_F(PanedPositionTest, NothingStoredYieldsNoPosition)
{
    const auto store = makeStore();

    EXPECT_FALSE(loadPanedPosition(store, "window.panes.sidebar").has_value());
}

TEST_F(PanedPositionTest, SavedPositionIsLoadedBack)
{
    auto store = makeStore();

    ASSERT_TRUE(savePanedPosition(store, "window.panes.sidebar", 240));

    EXPECT_EQ(loadPanedPosition(store, "window.panes.sidebar"), 240);
}

TEST_F(PanedPositionTest, PositionSurvivesReloadingTheStoreFromDisk)
{
    {
        auto store = makeStore();
        ASSERT_TRUE(savePanedPosition(store, "window.panes.sidebar", 310));
    }

    auto reopened = makeStore();
    reopened.load();

    EXPECT_EQ(loadPanedPosition(reopened, "window.panes.sidebar"), 310);
}

TEST_F(PanedPositionTest, KeysAreIndependent)
{
    auto store = makeStore();
    ASSERT_TRUE(savePanedPosition(store, "window.panes.sidebar", 200));
    ASSERT_TRUE(savePanedPosition(store, "window.panes.details", 450));

    EXPECT_EQ(loadPanedPosition(store, "window.panes.sidebar"), 200);
    EXPECT_EQ(loadPanedPosition(store, "window.panes.details"), 450);
}

TEST_F(PanedPositionTest, ZeroIsAValidPosition)
{
    auto store = makeStore();
    ASSERT_TRUE(savePanedPosition(store, "window.panes.sidebar", 0));

    EXPECT_EQ(loadPanedPosition(store, "window.panes.sidebar"), 0);
}

TEST_F(PanedPositionTest, OutOfRangePositionIsNotSaved)
{
    auto store = makeStore();

    EXPECT_FALSE(savePanedPosition(store, "window.panes.sidebar", -1));
    EXPECT_FALSE(savePanedPosition(store, "window.panes.sidebar", MAX_PANED_POSITION + 1));
    EXPECT_FALSE(store.contains("window.panes.sidebar"));
}

TEST_F(PanedPositionTest, OutOfRangeStoredPositionIsDropped)
{
    auto store = makeStore();

    store.set<int>("window.panes.sidebar", -5);
    EXPECT_FALSE(loadPanedPosition(store, "window.panes.sidebar").has_value());

    store.set<int>("window.panes.sidebar", MAX_PANED_POSITION + 1);
    EXPECT_FALSE(loadPanedPosition(store, "window.panes.sidebar").has_value());
}

TEST_F(PanedPositionTest, WrongValueTypeFallsBackToNoPosition)
{
    auto store = makeStore();
    store.set<std::string>("window.panes.sidebar", "wide");

    EXPECT_FALSE(loadPanedPosition(store, "window.panes.sidebar").has_value());
}
