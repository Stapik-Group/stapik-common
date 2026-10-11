#include "stapik/text/TextFolding.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    using namespace stapik::text;

    const std::string REPLACEMENT_CHARACTER = "\xEF\xBF\xBD";
}

TEST(TextFoldingTest, DecomposeSplitsPolishLettersIntoBaseAndMark)
{
    EXPECT_TRUE(decompose("ą") == U"a\u0328");
    EXPECT_TRUE(decompose("ó") == U"o\u0301");
    EXPECT_TRUE(decompose("ż") == U"z\u0307");
    EXPECT_TRUE(decompose("A") == U"A");
}

TEST(TextFoldingTest, DecomposeSplitsLettersWithAStroke)
{
    EXPECT_TRUE(decompose("ł") == U"l\u0335");
    EXPECT_TRUE(decompose("Ł") == U"L\u0335");
    EXPECT_TRUE(decompose("đ") == U"d\u0335");
    EXPECT_TRUE(decompose("Ø") == U"O\u0338");
}

TEST(TextFoldingTest, DecomposeAcceptsTextThatIsAlreadyDecomposed)
{
    EXPECT_TRUE(decompose("a\xCC\xA8") == decompose("ą"));
}

TEST(TextFoldingTest, ToUtf8RoundTripsDecomposedText)
{
    EXPECT_EQ(toUtf8(U"a\u0328z"), "a\xCC\xA8z");
    EXPECT_EQ(toUtf8(U""), "");
}

TEST(TextFoldingTest, FoldCaseLowersNonAsciiLetters)
{
    EXPECT_EQ(foldCase("ŁÓDŹ"), "łódź");
    EXPECT_EQ(foldCase("Hello"), "hello");
    EXPECT_EQ(foldCase(""), "");
}

TEST(TextFoldingTest, FoldCaseExpandsSharpS)
{
    EXPECT_EQ(foldCase("Straße"), "strasse");
}

TEST(TextFoldingTest, FoldForSearchRemovesPolishDiacritics)
{
    EXPECT_EQ(foldForSearch("Zażółć gęślą jaźń"), "zazolc gesla jazn");
    EXPECT_EQ(foldForSearch("ŁÓDŹ"), "lodz");
    EXPECT_EQ(foldForSearch("Łódź"), "lodz");
}

TEST(TextFoldingTest, FoldForSearchRemovesDiacriticsOfOtherLatinAndGreekLetters)
{
    EXPECT_EQ(foldForSearch("Müller"), "muller");
    EXPECT_EQ(foldForSearch("Été"), "ete");
    EXPECT_EQ(foldForSearch("İstanbul"), "istanbul");
    EXPECT_EQ(foldForSearch("Đorđe"), "dorde");
    EXPECT_EQ(foldForSearch("Ærø"), "æro");
    EXPECT_EQ(foldForSearch("ΆΛΦΑ"), "αλφα");
}

TEST(TextFoldingTest, FoldForSearchResolvesCompatibilityForms)
{
    EXPECT_EQ(foldForSearch("\xEF\xAC\x81x"), "fix");
    EXPECT_EQ(foldForSearch("m²"), "m2");
}

TEST(TextFoldingTest, FoldForSearchKeepsMarksThatAreLettersInOtherScripts)
{
    EXPECT_EQ(foldForSearch("Йога"), foldForSearch("йога"));
    EXPECT_NE(foldForSearch("Йога"), foldForSearch("Иога"));
}

TEST(TextFoldingTest, FoldForSearchTreatsDecomposedAndComposedTextAlike)
{
    EXPECT_EQ(foldForSearch("a\xCC\xA8"), "a");
    EXPECT_EQ(foldForSearch("ą"), "a");
}

TEST(TextFoldingTest, InvalidUtf8BecomesTheReplacementCharacter)
{
    EXPECT_EQ(foldForSearch("a\xFF" "z"), "a" + REPLACEMENT_CHARACTER + "z");
    EXPECT_EQ(foldCase("\xFF"), REPLACEMENT_CHARACTER);
    EXPECT_TRUE(decompose("\xC5") == U"\uFFFD");
}

TEST(TextFoldingTest, ContainsFoldedIgnoresCaseAndDiacritics)
{
    EXPECT_TRUE(containsFolded("Zażółć gęślą jaźń", "ZAZOL"));
    EXPECT_TRUE(containsFolded("Kraków", "krakow"));
    EXPECT_TRUE(containsFolded("Łódź", "od"));
    EXPECT_TRUE(containsFolded("Lodz", "łó"));
}

TEST(TextFoldingTest, ContainsFoldedDistinguishesDifferentLetters)
{
    EXPECT_FALSE(containsFolded("abc", "abd"));
    EXPECT_FALSE(containsFolded("Kraków", "krakau"));
    EXPECT_FALSE(containsFolded("", "a"));
}

TEST(TextFoldingTest, EmptyNeedleMatchesEverything)
{
    EXPECT_TRUE(containsFolded("Łódź", ""));
    EXPECT_TRUE(containsFolded("", ""));
}
