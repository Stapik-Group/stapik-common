#include "stapik/ui/window/WindowState.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::SettingsStore;
    using stapik::ui::WindowPosition;
    using stapik::ui::WindowState;

    class WindowStateTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-windowstate-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
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

TEST_F(WindowStateTest, NothingStoredYieldsNoState)
{
    const auto store = makeStore();

    EXPECT_FALSE(WindowState::load(store, "window").has_value());
}

TEST_F(WindowStateTest, SavedStateIsLoadedBack)
{
    auto store = makeStore();

    ASSERT_TRUE((WindowState{ .width = 1024, .height = 768, .maximized = false }.save(store, "window")));

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->width, 1024);
    EXPECT_EQ(state->height, 768);
    EXPECT_FALSE(state->maximized);
}

TEST_F(WindowStateTest, StateSurvivesReloadingTheStoreFromDisk)
{
    {
        auto store = makeStore();
        ASSERT_TRUE((WindowState{ .width = 900, .height = 600, .maximized = true }.save(store, "window")));
    }

    auto reopened = makeStore();
    reopened.load();

    const auto state = WindowState::load(reopened, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->width, 900);
    EXPECT_EQ(state->height, 600);
    EXPECT_TRUE(state->maximized);
}

TEST_F(WindowStateTest, KeysAreIndependent)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .width = 800, .height = 500 }.save(store, "window")));
    ASSERT_TRUE((WindowState{ .width = 400, .height = 300 }.save(store, "window.other")));

    EXPECT_EQ(WindowState::load(store, "window")->width, 800);
    EXPECT_EQ(WindowState::load(store, "window.other")->width, 400);
}

TEST_F(WindowStateTest, MaximizedWithoutSizeIsKept)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .maximized = true }.save(store, "window")));

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasSize());
    EXPECT_TRUE(state->maximized);
}

TEST_F(WindowStateTest, SavingWithoutSizeRemovesThePreviousSize)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .width = 800, .height = 500 }.save(store, "window")));
    ASSERT_TRUE((WindowState{ .maximized = true }.save(store, "window")));

    EXPECT_FALSE(WindowState::load(store, "window")->hasSize());
}

TEST_F(WindowStateTest, OutOfRangeSizeIsDropped)
{
    auto store = makeStore();
    store.set<int>("window.width", 5);
    store.set<int>("window.height", 700);

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasSize());

    store.set<int>("window.width", WindowState::MAX_DIMENSION + 1);
    EXPECT_FALSE(WindowState::load(store, "window")->hasSize());
}

TEST_F(WindowStateTest, WrongValueTypesFallBackToDefaults)
{
    auto store = makeStore();
    store.set<std::string>("window.width", "wide");
    store.set<std::string>("window.height", "tall");
    store.set<std::string>("window.maximized", "yes");

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasSize());
    EXPECT_FALSE(state->maximized);
}

TEST_F(WindowStateTest, SavedPositionIsLoadedBack)
{
    auto store = makeStore();

    ASSERT_TRUE((WindowState{ .position = WindowPosition{ .x = -1200, .y = 80 } }.save(store, "window")));

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_TRUE(state->hasPosition());
    EXPECT_EQ(state->position, (WindowPosition{ .x = -1200, .y = 80 }));
}

TEST_F(WindowStateTest, PositionSurvivesReloadingTheStoreFromDisk)
{
    {
        auto store = makeStore();
        ASSERT_TRUE((WindowState{ .width = 900, .height = 600, .position = WindowPosition{ .x = 40, .y = 30 } }.save(store, "window")));
    }

    auto reopened = makeStore();
    reopened.load();

    const auto state = WindowState::load(reopened, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->position, (WindowPosition{ .x = 40, .y = 30 }));
}

TEST_F(WindowStateTest, SavingWithoutPositionRemovesThePreviousPosition)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .position = WindowPosition{ .x = 10, .y = 20 } }.save(store, "window")));
    ASSERT_TRUE((WindowState{ .width = 800, .height = 500 }.save(store, "window")));

    EXPECT_FALSE(WindowState::load(store, "window")->hasPosition());
}

TEST_F(WindowStateTest, PositionWithoutSizeIsKept)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .position = WindowPosition{ .x = 10, .y = 20 } }.save(store, "window")));

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasSize());
    EXPECT_TRUE(state->hasPosition());
}

TEST_F(WindowStateTest, PositionWithOnlyOneCoordinateIsDropped)
{
    auto store = makeStore();
    store.set<int>("window.x", 100);

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasPosition());
}

TEST_F(WindowStateTest, OutOfRangePositionIsDropped)
{
    auto store = makeStore();
    store.set<int>("window.x", WindowState::MAX_COORDINATE + 1);
    store.set<int>("window.y", 0);
    EXPECT_FALSE(WindowState::load(store, "window")->hasPosition());

    store.set<int>("window.x", 0);
    store.set<int>("window.y", -WindowState::MAX_COORDINATE - 1);
    EXPECT_FALSE(WindowState::load(store, "window")->hasPosition());
}

TEST_F(WindowStateTest, WrongPositionValueTypesAreDropped)
{
    auto store = makeStore();
    store.set<std::string>("window.x", "left");
    store.set<std::string>("window.y", "top");

    const auto state = WindowState::load(store, "window");
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->hasPosition());
}

TEST_F(WindowStateTest, OutOfRangePositionIsNotSaved)
{
    auto store = makeStore();
    ASSERT_TRUE((WindowState{ .width = 800, .height = 500, .position = WindowPosition{ .x = WindowState::MAX_COORDINATE + 1, .y = 0 } }.save(store, "window")));

    EXPECT_FALSE(store.contains("window.x"));
    EXPECT_FALSE(store.contains("window.y"));
}
