#include "stapik/theme/Theme.hpp"

#include <gtest/gtest.h>

TEST(ThemeTest, EveryThemeSurvivesAFileStringRoundTrip)
{
    for (const auto theme : { Theme::Classic, Theme::Modern, Theme::ClassicPink, Theme::Dark })
        EXPECT_EQ(themeFromFileString(themeToFileString(theme)), theme);
}

TEST(ThemeTest, DarkUsesItsOwnFileString)
{
    EXPECT_EQ(themeToFileString(Theme::Dark), "dark");
    EXPECT_EQ(themeFromFileString("dark"), Theme::Dark);
}

TEST(ThemeTest, UnknownValueFallsBackToClassic)
{
    EXPECT_EQ(themeFromFileString("does-not-exist"), Theme::Classic);
}
