#include "stapik/locale/LocalizationEngine.hpp"

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
            auto pattern = (fs::temp_directory_path() / "stapik-engine-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;

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

TEST_F(LocalizationEngineTest, SetLocaleSelectsTheMatchingLanguage)
{
    LocalizationEngine engine(m_directory);

    engine.setLocale(Locale::PL);

    EXPECT_EQ(engine.languageCode(), "pl");
    EXPECT_EQ(engine.getLocale(), Locale::PL);
    EXPECT_EQ(engine.translate("dialog.cancel"), "Anuluj");
}

TEST_F(LocalizationEngineTest, GetLocaleOfLanguageOutsideTheEnumIsEnglish)
{
    LocalizationEngine engine(m_directory);

    engine.setLanguage("fr");

    EXPECT_EQ(engine.getLocale(), Locale::EN);
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
    LocalizationEngine engine(m_directory / "nope");

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
