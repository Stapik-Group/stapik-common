#include "stapik/locale/LanguageCode.hpp"

#include <gtest/gtest.h>

using stapik::locale::primaryLanguage;

TEST(LanguageCodeTest, KeepsPlainCodes)
{
    EXPECT_EQ(primaryLanguage("pl"), "pl");
    EXPECT_EQ(primaryLanguage(""), "");
}

TEST(LanguageCodeTest, DropsRegionEncodingAndModifier)
{
    EXPECT_EQ(primaryLanguage("pt-BR"), "pt");
    EXPECT_EQ(primaryLanguage("pl_PL"), "pl");
    EXPECT_EQ(primaryLanguage("pl_PL.UTF-8"), "pl");
    EXPECT_EQ(primaryLanguage("de@euro"), "de");
}

TEST(LanguageCodeTest, LowercasesTheLanguage)
{
    EXPECT_EQ(primaryLanguage("PL"), "pl");
    EXPECT_EQ(primaryLanguage("En-US"), "en");
}
