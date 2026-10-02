#include "stapik/ui/menu/RadioOptions.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    enum class Mode
    {
        Light,
        Dark,
        Auto
    };

    using stapik::ui::RadioOption;
    using stapik::ui::RadioOptions;

    RadioOptions<Mode> modes()
    {
        return RadioOptions<Mode>({
            { Mode::Light, "light" },
            { Mode::Dark, "dark" },
            { Mode::Auto, "auto" }
        });
    }
}

TEST(RadioOptionsTest, FindsIdByValue)
{
    const auto options = modes();

    ASSERT_NE(options.idOf(Mode::Dark), nullptr);
    EXPECT_EQ(*options.idOf(Mode::Dark), "dark");
}

TEST(RadioOptionsTest, FindsValueById)
{
    const auto options = modes();

    ASSERT_NE(options.valueOf("auto"), nullptr);
    EXPECT_EQ(*options.valueOf("auto"), Mode::Auto);
}

TEST(RadioOptionsTest, UnknownValueOrIdIsNotFound)
{
    const RadioOptions<std::string> options({ { "pl", "polish" } });

    EXPECT_EQ(options.idOf("de"), nullptr);
    EXPECT_EQ(options.valueOf("german"), nullptr);
}

TEST(RadioOptionsTest, KeepsOptionsInGivenOrder)
{
    const auto options = modes();

    ASSERT_EQ(options.all().size(), 3u);
    EXPECT_EQ(options.all().front().id, "light");
    EXPECT_EQ(options.all().back().id, "auto");
}

TEST(RadioOptionsTest, DefaultConstructedIsEmpty)
{
    const RadioOptions<Mode> options;

    EXPECT_TRUE(options.empty());
    EXPECT_EQ(options.idOf(Mode::Light), nullptr);
    EXPECT_EQ(options.valueOf("light"), nullptr);
}
