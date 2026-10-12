#include "stapik/text/Collation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <ranges>
#include <string>
#include <vector>

namespace
{
    using stapik::text::collationKey;
    using stapik::text::compareText;
    using stapik::text::TextLess;

    std::vector<std::string> sorted(std::vector<std::string> words, const std::string& languageCode)
    {
        std::ranges::sort(words, TextLess(languageCode));
        return words;
    }
}

TEST(CollationTest, PolishLettersFollowTheirBaseLetters)
{
    const std::vector<std::string> expected = {
        "abc", "ąbc", "cukier", "ćma", "lato", "Łódź", "ogórek", "olej",
        "sad", "śliwka", "tort", "zebra", "zło", "źle", "żaba"};

    const auto shuffled = std::vector<std::string>{
        "żaba", "zebra", "Łódź", "lato", "ćma", "cukier", "abc", "ąbc",
        "ogórek", "olej", "sad", "śliwka", "tort", "zło", "źle"};

    EXPECT_EQ(sorted(shuffled, "pl"), expected);
}

TEST(CollationTest, PolishLetterOutranksTheRestOfTheWord)
{
    EXPECT_LT(compareText("az", "ąa", "pl"), 0);
    EXPECT_LT(compareText("lz", "ła", "pl"), 0);
    EXPECT_LT(compareText("zz", "źa", "pl"), 0);
    EXPECT_LT(compareText("źz", "ża", "pl"), 0);
}

TEST(CollationTest, EnglishTreatsDiacriticsAsTieBreakers)
{
    const std::vector<std::string> shuffled = {"mama", "Łódź", "zebra", "apple"};

    EXPECT_EQ(sorted(shuffled, "en"), (std::vector<std::string>{"apple", "Łódź", "mama", "zebra"}));
    EXPECT_LT(compareText("Lodz", "Łódź", "en"), 0);
    EXPECT_LT(compareText("cote", "côte", "en"), 0);
}

TEST(CollationTest, GermanSortsUmlautsAsTheirBaseLetter)
{
    const std::vector<std::string> shuffled = {"Zebra", "Bär", "Äpfel", "Abend"};

    EXPECT_EQ(sorted(shuffled, "de"), (std::vector<std::string>{"Abend", "Äpfel", "Bär", "Zebra"}));
}

TEST(CollationTest, LigaturesSortAsTheirLetters)
{
    EXPECT_EQ(sorted({"coté", "cote", "cœur", "côte"}, "fr"), (std::vector<std::string>{"cœur", "cote", "coté", "côte"}));
    EXPECT_EQ(compareText("cœur", "coeur", "fr"), 0);
    EXPECT_GT(compareText("Œuf", "oeuf", "fr"), 0);
}

TEST(CollationTest, SharpSSortsAsDoubleS)
{
    EXPECT_EQ(compareText("Straße", "Strasse", "de"), 0);
    EXPECT_LT(compareText("Straße", "Strasz", "de"), 0);
    EXPECT_GT(compareText("Straße", "Strasr", "de"), 0);
}

TEST(CollationTest, CaseOnlyBreaksTies)
{
    EXPECT_LT(compareText("ab", "Ac", "pl"), 0);
    EXPECT_LT(compareText("anna", "Anna", "pl"), 0);
    EXPECT_LT(compareText("Anna", "anny", "pl"), 0);
    EXPECT_LT(compareText("ęga", "Ęga", "pl"), 0);
}

TEST(CollationTest, SymbolsComeBeforeDigitsAndDigitsBeforeLetters)
{
    EXPECT_EQ(sorted({"a", "1", " ", "_", "-"}, "pl"), (std::vector<std::string>{" ", "-", "_", "1", "a"}));
    EXPECT_LT(compareText("a1", "aa", "en"), 0);
}

TEST(CollationTest, ShorterTextComesBeforeItsContinuation)
{
    EXPECT_LT(compareText("ala", "alama", "pl"), 0);
    EXPECT_LT(compareText("", "a", "pl"), 0);
    EXPECT_EQ(compareText("", "", "pl"), 0);
}

TEST(CollationTest, LettersOfOtherAlphabetsComeAfterLatinOnes)
{
    EXPECT_LT(compareText("zebra", "альфа", "pl"), 0);
    EXPECT_LT(compareText("zebra", "αλφα", "en"), 0);
}

TEST(CollationTest, ComposedAndDecomposedTextCompareEqual)
{
    EXPECT_EQ(compareText("ą", "a\xCC\xA8", "pl"), 0);
    EXPECT_EQ(compareText("Łódź", "L\xCC\xB5" "o\xCC\x81" "dz\xCC\x81", "pl"), 0);
}

TEST(CollationTest, RegionAndCaseOfTheLanguageCodeAreIgnored)
{
    EXPECT_EQ(collationKey("Łódź", "pl_PL.UTF-8"), collationKey("Łódź", "pl"));
    EXPECT_EQ(collationKey("Łódź", "PL"), collationKey("Łódź", "pl"));
}

TEST(CollationTest, UnknownLanguageFollowsEnglish)
{
    EXPECT_EQ(collationKey("Łódź", ""), collationKey("Łódź", "en"));
    EXPECT_EQ(collationKey("Łódź", "fr"), collationKey("Łódź", "en"));
}

TEST(CollationTest, InvalidUtf8DoesNotBreakTheOrder)
{
    EXPECT_LT(compareText("a", "a\xFF", "pl"), 0);
    EXPECT_EQ(compareText("\xFF", "\xFF", "pl"), 0);
}

TEST(CollationTest, TextLessWorksAsAnOrderOfAnAssociativeContainer)
{
    std::map<std::string, int, TextLess> counts{TextLess("pl")};
    counts["żaba"] = 1;
    counts["zebra"] = 2;
    counts["ąbc"] = 3;
    counts["abc"] = 4;

    std::vector<std::string> keys;
    for (const auto &key: counts | std::views::keys)
        keys.push_back(key);

    EXPECT_EQ(keys, (std::vector<std::string>{"abc", "ąbc", "zebra", "żaba"}));
}
