#include "stapik/domain/CurrencyCatalog.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::domain::Currency;
    using stapik::domain::CurrencyCatalog;

    void writeText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    class CurrencyCatalogTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-currencies-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(CurrencyCatalogTest, LoadsCurrenciesFromAJsonArray)
{
    writeText(m_directory / "c.json", R"([
        {"code": "PLN", "symbol": "zł", "symbolBeforeAmount": false, "decimalPlaces": 2},
        {"code": "USD", "symbol": "$", "symbolBeforeAmount": true, "decimalPlaces": 2}
    ])");

    CurrencyCatalog catalog;

    ASSERT_TRUE(catalog.addFromFile(m_directory / "c.json"));
    ASSERT_EQ(catalog.currencies().size(), 2u);
    EXPECT_EQ(catalog.currencies()[0].code, "PLN");
    EXPECT_EQ(catalog.currencies()[1].symbol, "$");
    EXPECT_TRUE(catalog.currencies()[1].symbolBeforeAmount);
}

TEST_F(CurrencyCatalogTest, MissingFieldsGetDefaults)
{
    writeText(m_directory / "c.json", R"([{"code": "chf"}])");

    CurrencyCatalog catalog;
    catalog.addFromFile(m_directory / "c.json");

    const auto* currency = catalog.find("CHF");
    ASSERT_NE(currency, nullptr);
    EXPECT_EQ(currency->symbol, "CHF");
    EXPECT_FALSE(currency->symbolBeforeAmount);
    EXPECT_EQ(currency->decimalPlaces, 2);
}

TEST_F(CurrencyCatalogTest, CodeIsUppercased)
{
    writeText(m_directory / "c.json", R"([{"code": "eur", "symbol": "€"}])");

    CurrencyCatalog catalog;
    catalog.addFromFile(m_directory / "c.json");

    EXPECT_NE(catalog.find("EUR"), nullptr);
    EXPECT_EQ(catalog.find("eur"), nullptr);
}

TEST_F(CurrencyCatalogTest, InvalidEntriesAreSkippedAndTheRestLoads)
{
    writeText(m_directory / "c.json", R"([
        {"symbol": "?"},
        {"code": ""},
        {"code": "BAD", "decimalPlaces": 9},
        {"code": "NEG", "decimalPlaces": -1},
        {"code": "OK", "decimalPlaces": 0},
        "not an object"
    ])");

    CurrencyCatalog catalog;

    ASSERT_TRUE(catalog.addFromFile(m_directory / "c.json"));
    ASSERT_EQ(catalog.currencies().size(), 1u);
    EXPECT_EQ(catalog.currencies().front().code, "OK");
}

TEST_F(CurrencyCatalogTest, MissingNonArrayAndBrokenFilesAreRejected)
{
    writeText(m_directory / "object.json", R"({"code": "PLN"})");
    writeText(m_directory / "broken.json", "[ not json");

    CurrencyCatalog catalog;

    EXPECT_FALSE(catalog.addFromFile(m_directory / "missing.json"));
    EXPECT_FALSE(catalog.addFromFile(m_directory / "object.json"));
    EXPECT_FALSE(catalog.addFromFile(m_directory / "broken.json"));
    EXPECT_TRUE(catalog.currencies().empty());
}

TEST_F(CurrencyCatalogTest, LaterFileReplacesAndExtendsEarlierOnes)
{
    writeText(m_directory / "base.json", R"([
        {"code": "PLN", "symbol": "zł"},
        {"code": "EUR", "symbol": "€"}
    ])");
    writeText(m_directory / "app.json", R"([
        {"code": "EUR", "symbol": "EUR", "decimalPlaces": 3},
        {"code": "CZK", "symbol": "Kč"}
    ])");

    CurrencyCatalog catalog;
    catalog.addFromFile(m_directory / "base.json");
    catalog.addFromFile(m_directory / "app.json");

    ASSERT_EQ(catalog.currencies().size(), 3u);
    EXPECT_EQ(catalog.currencies()[1].code, "EUR");
    EXPECT_EQ(catalog.currencies()[1].decimalPlaces, 3);
    EXPECT_EQ(catalog.currencies()[2].code, "CZK");
}

TEST_F(CurrencyCatalogTest, AddReplacesByCode)
{
    CurrencyCatalog catalog;
    catalog.add(Currency{ "PLN", "zł", false, 2 });
    catalog.add(Currency{ "PLN", "PLN", true, 2 });

    ASSERT_EQ(catalog.currencies().size(), 1u);
    EXPECT_TRUE(catalog.find("PLN")->symbolBeforeAmount);
}

TEST_F(CurrencyCatalogTest, FindOfUnknownCodeIsNull)
{
    const CurrencyCatalog catalog;

    EXPECT_EQ(catalog.find("XXX"), nullptr);
}
