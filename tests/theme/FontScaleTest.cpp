#include "stapik/theme/FontScale.hpp"

#include <gtest/gtest.h>

using namespace stapik::theme;

TEST(FontScaleTest, FactorOfOneKeepsSizesAsTheyAre)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 14px; }", 1.0), "* { font-size: 14.0px; }");
}

TEST(FontScaleTest, EveryPixelFontSizeIsScaled)
{
    const auto scaled = scaleFontSizes("* { font-size: 14px; }\n.title { font-size:22px; }", 1.5);

    EXPECT_EQ(scaled, "* { font-size: 21.0px; }\n.title { font-size:33.0px; }");
}

TEST(FontScaleTest, FractionalResultKeepsOneDecimal)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 13px; }", 1.1), "* { font-size: 14.3px; }");
}

TEST(FontScaleTest, OtherLengthsAndUnitsAreLeftAlone)
{
    const std::string css = "button { padding: 14px; font-size: 1.2em; border-width: 2px; }";

    EXPECT_EQ(scaleFontSizes(css, 1.5), css);
}

TEST(FontScaleTest, FactorIsLimitedToTheSupportedRange)
{
    EXPECT_EQ(scaleFontSizes("* { font-size: 10px; }", 100.0), "* { font-size: 40.0px; }");
    EXPECT_EQ(scaleFontSizes("* { font-size: 10px; }", 0.01), "* { font-size: 2.5px; }");
}

TEST(FontScaleTest, BaseSizeIsTheFirstPixelFontSize)
{
    const auto base = baseFontSizePixels("* {\n    font-family: \"Inter\";\n    font-size: 13px;\n}\n.title { font-size: 22px; }");

    ASSERT_TRUE(base.has_value());
    EXPECT_DOUBLE_EQ(*base, 13.0);
}

TEST(FontScaleTest, BaseSizeIsMissingWithoutAPixelFontSize)
{
    EXPECT_FALSE(baseFontSizePixels("button { font-size: 1.2em; }").has_value());
    EXPECT_FALSE(baseFontSizePixels("").has_value());
}
