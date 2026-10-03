#include "stapik/domain/MoneyFormatter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using namespace stapik::domain;

    const std::string NBSP = "\xC2\xA0";

    Money pln(const std::int64_t minorUnits)
    {
        return Money(minorUnits, Currency{ "PLN", "zł", false, 2 });
    }

    Money usd(const std::int64_t minorUnits)
    {
        return Money(minorUnits, Currency{ "USD", "$", true, 2 });
    }

    Money yen(const std::int64_t minorUnits)
    {
        return Money(minorUnits, Currency{ "JPY", "¥", true, 0 });
    }
}

TEST(MoneyFormatterTest, PolishUsesCommaAndNoBreakSpaces)
{
    EXPECT_EQ(formatMoney(pln(123456), "pl"), "1" + NBSP + "234,56" + NBSP + "zł");
}

TEST(MoneyFormatterTest, EnglishUsesDotAndCommaWithSymbolFirst)
{
    EXPECT_EQ(formatMoney(usd(123456), "en"), "$1,234.56");
}

TEST(MoneyFormatterTest, GermanUsesDotGroupingAndCommaDecimals)
{
    const Money euros(123456, Currency{ "EUR", "€", false, 2 });

    EXPECT_EQ(formatMoney(euros, "de"), "1.234,56" + NBSP + "€");
}

TEST(MoneyFormatterTest, NegativeAmountsPutTheSignFirst)
{
    EXPECT_EQ(formatMoney(pln(-123456), "pl"), "-1" + NBSP + "234,56" + NBSP + "zł");
    EXPECT_EQ(formatMoney(usd(-5), "en"), "-$0.05");
}

TEST(MoneyFormatterTest, SmallAmountsKeepLeadingZeros)
{
    EXPECT_EQ(formatMoney(pln(5), "pl"), "0,05" + NBSP + "zł");
    EXPECT_EQ(formatMoney(pln(0), "pl"), "0,00" + NBSP + "zł");
    EXPECT_EQ(formatMoney(pln(100), "pl"), "1,00" + NBSP + "zł");
}

TEST(MoneyFormatterTest, OptionsControlSymbolAndGrouping)
{
    EXPECT_EQ(formatMoney(pln(123456), "en", { .withSymbol = false }), "1,234.56");
    EXPECT_EQ(formatMoney(pln(123456), "en", { .withSymbol = false, .groupThousands = false }), "1234.56");
}

TEST(MoneyFormatterTest, CurrenciesWithoutDecimalsHaveNoSeparator)
{
    EXPECT_EQ(formatMoney(yen(1234567), "en"), "¥1,234,567");
}

TEST(MoneyFormatterTest, MostNegativeValueDoesNotOverflow)
{
    EXPECT_EQ(formatAmount(std::numeric_limits<std::int64_t>::min(), 2, "en"), "-92,233,720,368,547,758.08");
}

TEST(MoneyFormatterTest, UnknownLanguageFallsBackToEnglishFormat)
{
    EXPECT_EQ(formatAmount(123456, 2, "xx"), "1,234.56");
    EXPECT_EQ(formatAmount(123456, 2, ""), "1,234.56");
}

TEST(MoneyFormatterTest, LanguageCodeMayBeAFullLocaleName)
{
    EXPECT_EQ(formatAmount(123456, 2, "de_DE.UTF-8"), "1.234,56");
    EXPECT_EQ(formatAmount(123456, 2, "pl-PL"), "1" + NBSP + "234,56");
    EXPECT_EQ(formatAmount(123456, 2, "PL"), "1" + NBSP + "234,56");
}

TEST(ParseAmountTest, AcceptsEitherDecimalSeparator)
{
    EXPECT_EQ(parseAmount("250,00", 2), 25000);
    EXPECT_EQ(parseAmount("250.00", 2), 25000);
    EXPECT_EQ(parseAmount("250", 2), 25000);
    EXPECT_EQ(parseAmount("250,5", 2), 25050);
    EXPECT_EQ(parseAmount("0,05", 2), 5);
}

TEST(ParseAmountTest, AcceptsPartialInput)
{
    EXPECT_EQ(parseAmount(".5", 2), 50);
    EXPECT_EQ(parseAmount("5,", 2), 500);
}

TEST(ParseAmountTest, UnderstandsGroupedNumbers)
{
    EXPECT_EQ(parseAmount("1 234,56", 2), 123456);
    EXPECT_EQ(parseAmount("1.234,56", 2), 123456);
    EXPECT_EQ(parseAmount("1,234.56", 2), 123456);
    EXPECT_EQ(parseAmount("1 234", 2), 123400);
    EXPECT_EQ(parseAmount("1.234.567", 2), 123456700);
    EXPECT_EQ(parseAmount("1,234,567.89", 2), 123456789);
}

TEST(ParseAmountTest, IgnoresNoBreakSpaces)
{
    EXPECT_EQ(parseAmount("1" + NBSP + "234,56", 2), 123456);
    EXPECT_EQ(parseAmount("1\xE2\x80\xAF" "234,56", 2), 123456);
}

TEST(ParseAmountTest, HandlesSigns)
{
    EXPECT_EQ(parseAmount("-12,34", 2), -1234);
    EXPECT_EQ(parseAmount("+5", 2), 500);
}

TEST(ParseAmountTest, RejectsMalformedInput)
{
    const std::vector<std::string> invalid = {
        "", " ", "abc", "12a", "-", "+", ".", ",", "--5", "1,2,3", "1.234,56,78", "1,234.56.78", "1.2.3", "12 zł"
    };

    for (const auto& text : invalid)
        EXPECT_FALSE(parseAmount(text, 2).has_value()) << text;
}

TEST(ParseAmountTest, RejectsMoreFractionDigitsThanTheCurrencyHas)
{
    EXPECT_FALSE(parseAmount("1,234", 2).has_value());
    EXPECT_FALSE(parseAmount("0,001", 2).has_value());
    EXPECT_FALSE(parseAmount("250,5", 0).has_value());
    EXPECT_EQ(parseAmount("1,234", 3), 1234);
}

TEST(ParseAmountTest, ZeroDecimalPlaces)
{
    EXPECT_EQ(parseAmount("250", 0), 250);
    EXPECT_EQ(parseAmount("1 234", 0), 1234);
}

TEST(ParseAmountTest, WithoutDecimalPlacesASingleSeparatorIsGrouping)
{
    EXPECT_EQ(parseAmount("123,456", 0), 123456);
    EXPECT_EQ(parseAmount("123.456", 0), 123456);
    EXPECT_FALSE(parseAmount("12,34", 0).has_value());
}

TEST(ParseAmountTest, RejectsValuesOutsideTheRange)
{
    EXPECT_EQ(parseAmount("9223372036854775807", 0), std::numeric_limits<std::int64_t>::max());
    EXPECT_FALSE(parseAmount("9223372036854775808", 0).has_value());
    EXPECT_EQ(parseAmount("92233720368547758,07", 2), std::numeric_limits<std::int64_t>::max());
    EXPECT_FALSE(parseAmount("92233720368547758,08", 2).has_value());
}

TEST(ParseAmountTest, RejectsInvalidDecimalPlaces)
{
    EXPECT_FALSE(parseAmount("1", -1).has_value());
    EXPECT_FALSE(parseAmount("1", MAX_DECIMAL_PLACES + 1).has_value());
}

TEST(ParseAmountTest, FormattedAmountsParseBack)
{
    const std::vector<std::int64_t> values = { 0, 5, 99, 100, 123456, 1000000, -987654321, 99999999999 };

    for (const auto* language : { "pl", "en", "de" })
    {
        for (const auto value : values)
        {
            for (const int decimals : { 0, 2, 3 })
            {
                const auto text = formatAmount(value, decimals, language);
                EXPECT_EQ(parseAmount(text, decimals), value) << language << " " << decimals << " " << text;
            }
        }
    }
}
