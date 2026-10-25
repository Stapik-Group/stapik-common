#include "stapik/locale/LocalizationEngine.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    void writeText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    class LocalizationEngineTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-engine");

            writeText(m_directory / "en.json", R"({
                "language.name": "English",
                "dialog.ok": "OK",
                "dialog.cancel": "Cancel",
                "greeting": "Hello, {name}!",
                "pair": "{first} and {second}",
                "repeat": "{x}{x}",
                "english.only": "Only in English"
            })");
            writeText(m_directory / "pl.json", R"({
                "language.name": "Polski",
                "dialog.ok": "OK-pl",
                "dialog.cancel": "Anuluj",
                "greeting": "Czesc, {name}!"
            })");
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(LocalizationEngineTest, TranslatesInTheSelectedLanguage)
{
    LocalizationEngine engine(m_directory);

    engine.setLanguage("pl");
    EXPECT_EQ(engine.translate("dialog.cancel"), "Anuluj");

    engine.setLanguage("en");
    EXPECT_EQ(engine.translate("dialog.cancel"), "Cancel");
}

TEST_F(LocalizationEngineTest, LoadsLanguagesOutsideTheLocaleEnum)
{
    writeText(m_directory / "fr.json", R"({"language.name": "Francais", "dialog.ok": "D'accord"})");

    LocalizationEngine engine(m_directory);
    engine.setLanguage("fr");

    EXPECT_EQ(engine.translate("dialog.ok"), "D'accord");
    EXPECT_EQ(engine.languages().size(), 3u);
}

TEST_F(LocalizationEngineTest, MissingKeyFallsBackToEnglish)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translate("english.only"), "Only in English");
}

TEST_F(LocalizationEngineTest, UnknownKeyReturnsTheKey)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translate("no.such.key"), "no.such.key");
}

TEST_F(LocalizationEngineTest, UnknownLanguageFallsBackToEnglish)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("xx");

    EXPECT_EQ(engine.translate("dialog.ok"), "OK");
}

TEST_F(LocalizationEngineTest, ReplacesPlaceholders)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translate("greeting", { { "name", "Ola" } }), "Czesc, Ola!");
    EXPECT_EQ(engine.translate("pair", { { "first", "A" }, { "second", "B" } }), "A and B");
    EXPECT_EQ(engine.translate("repeat", { { "x", "ab" } }), "abab");
}

TEST_F(LocalizationEngineTest, UnknownPlaceholdersAreKept)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("en");

    EXPECT_EQ(engine.translate("pair", { { "first", "A" } }), "A and {second}");
    EXPECT_EQ(engine.translate("greeting"), "Hello, {name}!");
}

TEST_F(LocalizationEngineTest, PlaceholdersAreAlsoReplacedInTheEnglishFallback)
{
    writeText(m_directory / "pl.json", R"({"language.name": "Polski"})");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translate("greeting", { { "name", "Ola" } }), "Hello, Ola!");
}

TEST_F(LocalizationEngineTest, LanguageCodeReflectsTheSelection)
{
    LocalizationEngine engine(m_directory);

    engine.setLanguage("fr");

    EXPECT_EQ(engine.languageCode(), "fr");
}

TEST_F(LocalizationEngineTest, NonStringEntriesAreIgnoredAndTheRestIsLoaded)
{
    writeText(m_directory / "pl.json", R"({"dialog.ok": 5, "dialog.cancel": "Anuluj"})");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translate("dialog.cancel"), "Anuluj");
    EXPECT_EQ(engine.translate("dialog.ok"), "OK");
}

TEST_F(LocalizationEngineTest, BrokenFileDoesNotAffectOtherLanguages)
{
    writeText(m_directory / "pl.json", "{ broken");
    LocalizationEngine engine(m_directory);

    engine.setLanguage("pl");
    EXPECT_EQ(engine.translate("dialog.cancel"), "Cancel");

    engine.setLanguage("en");
    EXPECT_EQ(engine.translate("dialog.ok"), "OK");
}

TEST_F(LocalizationEngineTest, MissingDirectoryTranslatesToKeys)
{
    const LocalizationEngine engine(m_directory / "nope");

    EXPECT_EQ(engine.translate("dialog.ok"), "dialog.ok");
    EXPECT_TRUE(engine.languages().empty());
}

TEST_F(LocalizationEngineTest, AdditionalDirectoryOverridesAndExtendsTranslations)
{
    const auto appDirectory = m_directory / "app";
    writeText(appDirectory / "en.json", R"({"dialog.ok": "Okay", "app.title": "My App"})");
    writeText(appDirectory / "de.json", R"({"language.name": "Deutsch", "dialog.ok": "Gut"})");

    LocalizationEngine engine(m_directory);
    engine.addLocalesDirectory(appDirectory);

    engine.setLanguage("en");
    EXPECT_EQ(engine.translate("dialog.ok"), "Okay");
    EXPECT_EQ(engine.translate("dialog.cancel"), "Cancel");
    EXPECT_EQ(engine.translate("app.title"), "My App");

    engine.setLanguage("de");
    EXPECT_EQ(engine.translate("dialog.ok"), "Gut");
}

TEST_F(LocalizationEngineTest, AcceptsStdStringKeys)
{
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");
    const std::string key = "dialog.cancel";

    EXPECT_EQ(engine.translate(key), "Anuluj");
}

TEST_F(LocalizationEngineTest, PluralFormIsChosenByTheLanguageRules)
{
    writeText(m_directory / "pl.json", R"({
        "transaction.one": "{count} transakcja",
        "transaction.few": "{count} transakcje",
        "transaction.many": "{count} transakcji"
    })");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translatePlural("transaction", 1), "1 transakcja");
    EXPECT_EQ(engine.translatePlural("transaction", 2), "2 transakcje");
    EXPECT_EQ(engine.translatePlural("transaction", 5), "5 transakcji");
    EXPECT_EQ(engine.translatePlural("transaction", 12), "12 transakcji");
    EXPECT_EQ(engine.translatePlural("transaction", 22), "22 transakcje");
    EXPECT_EQ(engine.translatePlural("transaction", 0), "0 transakcji");
}

TEST_F(LocalizationEngineTest, PluralFormFallsBackToOtherWhenTheCategoryIsMissing)
{
    writeText(m_directory / "pl.json", R"({"file.one": "{count} plik", "file.other": "{count} pliki"})");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translatePlural("file", 1), "1 plik");
    EXPECT_EQ(engine.translatePlural("file", 3), "3 pliki");
}

TEST_F(LocalizationEngineTest, PluralFormFallsBackToEnglishWithEnglishRules)
{
    writeText(m_directory / "en.json", R"({"item.one": "{count} item", "item.other": "{count} items"})");
    writeText(m_directory / "pl.json", R"({"language.name": "Polski"})");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("pl");

    EXPECT_EQ(engine.translatePlural("item", 1), "1 item");
    EXPECT_EQ(engine.translatePlural("item", 2), "2 items");
}

TEST_F(LocalizationEngineTest, PluralCountCanBeOverriddenAndCombinedWithOtherArguments)
{
    writeText(m_directory / "en.json", R"({
        "deleted.one": "{name} deleted {count} entry",
        "deleted.other": "{name} deleted {count} entries"
    })");
    LocalizationEngine engine(m_directory);
    engine.setLanguage("en");

    EXPECT_EQ(engine.translatePlural("deleted", 3, { { "name", "Ola" } }), "Ola deleted 3 entries");
    EXPECT_EQ(engine.translatePlural("deleted", 1000, { { "name", "Ola" }, { "count", "1,000" } }), "Ola deleted 1,000 entries");
}

TEST_F(LocalizationEngineTest, UnknownPluralKeyReturnsTheKey)
{
    const LocalizationEngine engine(m_directory);

    EXPECT_EQ(engine.translatePlural("no.such.key", 2), "no.such.key");
}
