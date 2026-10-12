#include "stapik/theme/FontScale.hpp"

#include <gtest/gtest.h>

using namespace stapik::theme;

TEST(FontScaleTest, DefaultScaleKeepsSizesAsTheyAre)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 14px; }", DEFAULT_FONT_SCALE_PERCENT), "* { font-size: 14.0px; }");
}

TEST(FontScaleTest, EveryPixelFontSizeIsScaled)
{
    const auto scaled = scaleFontSizes("* { font-size: 14px; }\n.title { font-size:22px; }", 150);

    EXPECT_EQ(scaled, "* { font-size: 21.0px; }\n.title { font-size:33.0px; }");
}

TEST(FontScaleTest, FractionalResultKeepsOneDecimal)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 13px; }", 110), "* { font-size: 14.3px; }");
}

TEST(FontScaleTest, OtherLengthsAndUnitsAreLeftAlone)
{
    const std::string css = "button { padding: 14px; font-size: 1.2em; border-width: 2px; }";

    EXPECT_EQ(scaleFontSizes(css, 150), css);
}

TEST(FontScaleTest, ScaleIsLimitedToTheSupportedRange)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 10px; }", 1000), "* { font-size: 20.0px; }");
    EXPECT_EQ(scaleFontSizes("* { font-size: 10px; }", 1), "* { font-size: 7.0px; }");
}

TEST(FontScaleTest, ClampKeepsValuesInsideTheRange)
{
    EXPECT_EQ(clampFontScalePercent(MIN_FONT_SCALE_PERCENT - 1), MIN_FONT_SCALE_PERCENT);
    EXPECT_EQ(clampFontScalePercent(120), 120);
    EXPECT_EQ(clampFontScalePercent(MAX_FONT_SCALE_PERCENT + 1), MAX_FONT_SCALE_PERCENT);
}
