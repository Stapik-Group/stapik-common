#include "stapik/theme/FontChoice.hpp"

#include <gtest/gtest.h>

using namespace stapik::theme;

TEST(FontChoiceTest, FormatPutsTheSizeAfterTheFamily)
{
    EXPECT_EQ(formatFontChoice({ .family = "Inter", .sizePoints = 11.0 }), "Inter 11");
    EXPECT_EQ(formatFontChoice({ .family = "Noto Sans", .sizePoints = 10.5 }), "Noto Sans 10.5");
}

TEST(FontChoiceTest, ParseReadsTheFamilyAndTheSize)
{
    const auto choice = parseFontChoice("Noto Sans Mono 10.5");

    ASSERT_TRUE(choice.has_value());
    EXPECT_EQ(choice->family, "Noto Sans Mono");
    EXPECT_DOUBLE_EQ(choice->sizePoints, 10.5);
}

TEST(FontChoiceTest, ParseUndoesFormat)
{
    const FontChoice original{ .family = "DejaVu Sans", .sizePoints = 12.0 };

    EXPECT_EQ(parseFontChoice(formatFontChoice(original)), original);
}

TEST(FontChoiceTest, ParseIgnoresSurroundingSpaces)
{
    const auto choice = parseFontChoice("  Inter   11  ");

    ASSERT_TRUE(choice.has_value());
    EXPECT_EQ(choice->family, "Inter");
    EXPECT_DOUBLE_EQ(choice->sizePoints, 11.0);
}

TEST(FontChoiceTest, ParseKeepsADigitInTheFamilyName)
{
    const auto choice = parseFontChoice("Font 5 12");

    ASSERT_TRUE(choice.has_value());
    EXPECT_EQ(choice->family, "Font 5");
    EXPECT_DOUBLE_EQ(choice->sizePoints, 12.0);
}

TEST(FontChoiceTest, ParseMovesTheSizeIntoTheSupportedRange)
{
    EXPECT_DOUBLE_EQ(parseFontChoice("Inter 1")->sizePoints, MIN_FONT_SIZE_POINTS);
    EXPECT_DOUBLE_EQ(parseFontChoice("Inter 500")->sizePoints, MAX_FONT_SIZE_POINTS);
}

TEST(FontChoiceTest, ParseRejectsWhatIsNotAFont)
{
    EXPECT_FALSE(parseFontChoice("").has_value());
    EXPECT_FALSE(parseFontChoice("   ").has_value());
    EXPECT_FALSE(parseFontChoice("Inter").has_value());
    EXPECT_FALSE(parseFontChoice("Inter large").has_value());
    EXPECT_FALSE(parseFontChoice("Inter 11pt").has_value());
    EXPECT_FALSE(parseFontChoice("11").has_value());
    EXPECT_FALSE(parseFontChoice(std::string("Int\ner 11")).has_value());
}

TEST(FontChoiceTest, FamilyCssIsQuotedWithAGenericFallback)
{
    EXPECT_EQ(fontFamilyCss("Noto Sans"), "\"Noto Sans\", sans-serif");
}

TEST(FontChoiceTest, FamilyCssEscapesQuotesAndBackslashes)
{
    EXPECT_EQ(fontFamilyCss("A\"B\\C"), "\"A\\\"B\\\\C\", sans-serif");
}
