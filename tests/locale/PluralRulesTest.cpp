#include "stapik/locale/PluralRules.hpp"

#include <gtest/gtest.h>

namespace
{
    using stapik::locale::PluralCategory;
    using stapik::locale::pluralCategory;
    using stapik::locale::pluralCategoryName;
}

TEST(PluralRulesTest, PolishHasOneFewAndMany)
{
    EXPECT_EQ(pluralCategory("pl", 1), PluralCategory::One);
    EXPECT_EQ(pluralCategory("pl", 2), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("pl", 4), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("pl", 0), PluralCategory::Many);
    EXPECT_EQ(pluralCategory("pl", 5), PluralCategory::Many);
    EXPECT_EQ(pluralCategory("pl", 21), PluralCategory::Many);
    EXPECT_EQ(pluralCategory("pl", 22), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("pl", 102), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("pl", 1000), PluralCategory::Many);
}

TEST(PluralRulesTest, PolishTeensAreMany)
{
    for (const long long count : { 11, 12, 13, 14, 111, 112, 113, 114 })
        EXPECT_EQ(pluralCategory("pl", count), PluralCategory::Many) << count;
}

TEST(PluralRulesTest, EnglishAndGermanHaveOneAndOther)
{
    for (const auto language : { "en", "de" })
    {
        EXPECT_EQ(pluralCategory(language, 1), PluralCategory::One) << language;
        EXPECT_EQ(pluralCategory(language, 0), PluralCategory::Other) << language;
        EXPECT_EQ(pluralCategory(language, 2), PluralCategory::Other) << language;
        EXPECT_EQ(pluralCategory(language, 21), PluralCategory::Other) << language;
    }
}

TEST(PluralRulesTest, EastSlavicLanguagesUseTheLastDigits)
{
    for (const auto language : { "ru", "uk" })
    {
        EXPECT_EQ(pluralCategory(language, 1), PluralCategory::One) << language;
        EXPECT_EQ(pluralCategory(language, 21), PluralCategory::One) << language;
        EXPECT_EQ(pluralCategory(language, 11), PluralCategory::Many) << language;
        EXPECT_EQ(pluralCategory(language, 3), PluralCategory::Few) << language;
        EXPECT_EQ(pluralCategory(language, 12), PluralCategory::Many) << language;
        EXPECT_EQ(pluralCategory(language, 0), PluralCategory::Many) << language;
    }
}

TEST(PluralRulesTest, SouthSlavicLanguagesHaveNoManyForIntegers)
{
    EXPECT_EQ(pluralCategory("hr", 21), PluralCategory::One);
    EXPECT_EQ(pluralCategory("hr", 3), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("hr", 5), PluralCategory::Other);
    EXPECT_EQ(pluralCategory("hr", 11), PluralCategory::Other);
}

TEST(PluralRulesTest, CzechAndSlovakHaveOneFewAndOther)
{
    for (const auto language : { "cs", "sk" })
    {
        EXPECT_EQ(pluralCategory(language, 1), PluralCategory::One) << language;
        EXPECT_EQ(pluralCategory(language, 3), PluralCategory::Few) << language;
        EXPECT_EQ(pluralCategory(language, 5), PluralCategory::Other) << language;
        EXPECT_EQ(pluralCategory(language, 0), PluralCategory::Other) << language;
    }
}

TEST(PluralRulesTest, FrenchTreatsZeroAsOne)
{
    EXPECT_EQ(pluralCategory("fr", 0), PluralCategory::One);
    EXPECT_EQ(pluralCategory("fr", 1), PluralCategory::One);
    EXPECT_EQ(pluralCategory("fr", 2), PluralCategory::Other);
    EXPECT_EQ(pluralCategory("fr", 1000000), PluralCategory::Many);
}

TEST(PluralRulesTest, SpanishAndItalianHaveManyForWholeMillions)
{
    EXPECT_EQ(pluralCategory("es", 1), PluralCategory::One);
    EXPECT_EQ(pluralCategory("es", 0), PluralCategory::Other);
    EXPECT_EQ(pluralCategory("it", 2000000), PluralCategory::Many);
    EXPECT_EQ(pluralCategory("it", 1999999), PluralCategory::Other);
}

TEST(PluralRulesTest, LanguagesWithoutPluralsAlwaysUseOther)
{
    for (const auto language : { "ja", "zh", "ko", "vi" })
    {
        EXPECT_EQ(pluralCategory(language, 1), PluralCategory::Other) << language;
        EXPECT_EQ(pluralCategory(language, 2), PluralCategory::Other) << language;
    }
}

TEST(PluralRulesTest, UnknownLanguageFollowsTheEnglishRule)
{
    EXPECT_EQ(pluralCategory("xx", 1), PluralCategory::One);
    EXPECT_EQ(pluralCategory("xx", 2), PluralCategory::Other);
    EXPECT_EQ(pluralCategory("", 1), PluralCategory::One);
}

TEST(PluralRulesTest, IgnoresRegionAndCase)
{
    EXPECT_EQ(pluralCategory("pl-PL", 2), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("PL_pl", 5), PluralCategory::Many);
    EXPECT_EQ(pluralCategory("pt-BR", 0), PluralCategory::One);
}

TEST(PluralRulesTest, NegativeCountsUseTheirMagnitude)
{
    EXPECT_EQ(pluralCategory("pl", -1), PluralCategory::One);
    EXPECT_EQ(pluralCategory("pl", -2), PluralCategory::Few);
    EXPECT_EQ(pluralCategory("en", -1), PluralCategory::One);
}

TEST(PluralRulesTest, CategoryNamesMatchCldr)
{
    EXPECT_EQ(pluralCategoryName(PluralCategory::Zero), "zero");
    EXPECT_EQ(pluralCategoryName(PluralCategory::One), "one");
    EXPECT_EQ(pluralCategoryName(PluralCategory::Two), "two");
    EXPECT_EQ(pluralCategoryName(PluralCategory::Few), "few");
    EXPECT_EQ(pluralCategoryName(PluralCategory::Many), "many");
    EXPECT_EQ(pluralCategoryName(PluralCategory::Other), "other");
}
