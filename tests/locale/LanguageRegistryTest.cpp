#include "stapik/locale/LanguageRegistry.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::locale::LanguageRegistry;

    void writeText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    class LanguageRegistryTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-languages-XXXXXX").string();
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

TEST_F(LanguageRegistryTest, DiscoversLanguagesSortedByCode)
{
    writeText(m_directory / "pl.json", R"({"language.name": "Polski"})");
    writeText(m_directory / "de.json", R"({"language.name": "Deutsch"})");
    writeText(m_directory / "en.json", R"({"language.name": "English"})");

    const LanguageRegistry registry(m_directory);

    ASSERT_EQ(registry.languages().size(), 3u);
    EXPECT_EQ(registry.languages()[0].code, "de");
    EXPECT_EQ(registry.languages()[1].code, "en");
    EXPECT_EQ(registry.languages()[2].code, "pl");
}

TEST_F(LanguageRegistryTest, DisplayNameComesFromTheLanguageNameKey)
{
    writeText(m_directory / "pl.json", R"({"language.name": "Polski", "dialog.ok": "OK"})");

    const LanguageRegistry registry(m_directory);

    ASSERT_NE(registry.find("pl"), nullptr);
    EXPECT_EQ(registry.find("pl")->displayName, "Polski");
}

TEST_F(LanguageRegistryTest, DisplayNameFallsBackToUppercasedCode)
{
    writeText(m_directory / "fr.json", R"({"dialog.ok": "OK"})");
    writeText(m_directory / "es.json", "not json at all");

    const LanguageRegistry registry(m_directory);

    ASSERT_NE(registry.find("fr"), nullptr);
    EXPECT_EQ(registry.find("fr")->displayName, "FR");
    ASSERT_NE(registry.find("es"), nullptr);
    EXPECT_EQ(registry.find("es")->displayName, "ES");
}

TEST_F(LanguageRegistryTest, CodeIsTheLowercasedFileStem)
{
    writeText(m_directory / "PL.json", "{}");

    const LanguageRegistry registry(m_directory);

    EXPECT_TRUE(registry.contains("pl"));
    EXPECT_FALSE(registry.contains("PL"));
}

TEST_F(LanguageRegistryTest, IgnoresOtherFilesHiddenFilesAndDirectories)
{
    writeText(m_directory / "en.json", "{}");
    writeText(m_directory / "notes.txt", "x");
    writeText(m_directory / ".hidden.json", "{}");
    fs::create_directories(m_directory / "sub.json");

    const LanguageRegistry registry(m_directory);

    ASSERT_EQ(registry.languages().size(), 1u);
    EXPECT_EQ(registry.languages().front().code, "en");
}

TEST_F(LanguageRegistryTest, MissingDirectoryYieldsNoLanguages)
{
    const LanguageRegistry registry(m_directory / "does-not-exist");

    EXPECT_TRUE(registry.languages().empty());
    EXPECT_EQ(registry.find("en"), nullptr);
}

TEST_F(LanguageRegistryTest, SecondDirectoryLayersFilesAndOverridesDisplayName)
{
    const auto base = m_directory / "base";
    const auto app = m_directory / "app";
    writeText(base / "en.json", R"({"language.name": "English"})");
    writeText(base / "pl.json", R"({"language.name": "Polski"})");
    writeText(app / "en.json", R"json({"language.name": "English (app)"})json");
    writeText(app / "de.json", R"({"language.name": "Deutsch"})");

    LanguageRegistry registry(base);
    registry.addDirectory(app);

    ASSERT_EQ(registry.languages().size(), 3u);
    const auto* english = registry.find("en");
    ASSERT_NE(english, nullptr);
    ASSERT_EQ(english->files.size(), 2u);
    EXPECT_EQ(english->files[0], base / "en.json");
    EXPECT_EQ(english->files[1], app / "en.json");
    EXPECT_EQ(english->displayName, "English (app)");
    EXPECT_TRUE(registry.contains("de"));
    EXPECT_EQ(registry.languages().front().code, "de");
}
