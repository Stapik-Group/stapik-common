#include "stapik/locale/SystemLanguage.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    using stapik::locale::matchLanguage;

    const std::vector<std::string> AVAILABLE = { "de", "en", "pl" };
}

TEST(SystemLanguageTest, MatchesTheLanguagePartOfAPosixLocale)
{
    EXPECT_EQ(matchLanguage({ "pl_PL.UTF-8", "pl_PL", "pl", "C" }, AVAILABLE), "pl");
    EXPECT_EQ(matchLanguage({ "de_AT.UTF-8" }, AVAILABLE), "de");
}

TEST(SystemLanguageTest, AcceptsDashesAndModifiers)
{
    EXPECT_EQ(matchLanguage({ "en-US" }, AVAILABLE), "en");
    EXPECT_EQ(matchLanguage({ "de@euro" }, AVAILABLE), "de");
}

TEST(SystemLanguageTest, IgnoresCase)
{
    EXPECT_EQ(matchLanguage({ "PL" }, AVAILABLE), "pl");
}

TEST(SystemLanguageTest, SkipsUnavailableLanguagesInPreferenceOrder)
{
    EXPECT_EQ(matchLanguage({ "fr_FR.UTF-8", "fr", "pl_PL" }, AVAILABLE), "pl");
}

TEST(SystemLanguageTest, FallsBackWhenNothingMatches)
{
    EXPECT_EQ(matchLanguage({ "fr_FR", "es" }, AVAILABLE), "en");
    EXPECT_EQ(matchLanguage({ "fr_FR" }, AVAILABLE, "de"), "de");
}

TEST(SystemLanguageTest, SkipsCAndPosix)
{
    EXPECT_EQ(matchLanguage({ "C", "POSIX" }, AVAILABLE), "en");
    EXPECT_EQ(matchLanguage({ "C", "de" }, AVAILABLE), "de");
}

TEST(SystemLanguageTest, EmptyPreferenceListFallsBack)
{
    EXPECT_EQ(matchLanguage({}, AVAILABLE), "en");
    EXPECT_EQ(matchLanguage({ "" }, AVAILABLE), "en");
}

TEST(SystemLanguageTest, NoAvailableLanguagesFallsBack)
{
    EXPECT_EQ(matchLanguage({ "pl_PL" }, {}), "en");
}

TEST(SystemLanguageTest, SystemLanguageIsAvailableOrTheFallback)
{
    const auto code = stapik::locale::systemLanguageCode(AVAILABLE, "en");

    EXPECT_TRUE(std::ranges::find(AVAILABLE, code) != AVAILABLE.end());
}
