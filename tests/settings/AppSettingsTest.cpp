#include "stapik/settings/AppSettings.hpp"

#include "support/ScopedEnvironment.hpp"

#include <gtest/gtest.h>

#include <cctype>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::appSettingsStore;
    using stapik::settings::importLegacyTextSetting;
    using stapik::settings::LoadStatus;
    using stapik::settings::SettingsStore;

    void writeText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    std::string nextAppName()
    {
        static int counter = 0;
        return "stapik-test-app-" + std::to_string(counter++);
    }

    class AppSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-appsettings-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;

            setenv("XDG_CONFIG_HOME", (m_directory / "config").c_str(), 1);
            setenv("XDG_DATA_HOME", (m_directory / "data").c_str(), 1);
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;

    private:
        stapik::test::ScopedEnvironment m_configHome{ "XDG_CONFIG_HOME" };
        stapik::test::ScopedEnvironment m_dataHome{ "XDG_DATA_HOME" };
    };
}

TEST_F(AppSettingsTest, StoreLivesInTheConfigDirectoryOfTheApp)
{
    const auto appName = nextAppName();

    const auto& store = appSettingsStore(appName);

    EXPECT_EQ(store.filePath(), m_directory / "config" / appName / "settings.json");
}

TEST_F(AppSettingsTest, SameAppNameReturnsTheSameStore)
{
    const auto appName = nextAppName();

    EXPECT_EQ(&appSettingsStore(appName), &appSettingsStore(appName));
}

TEST_F(AppSettingsTest, DifferentAppNamesReturnDifferentStores)
{
    EXPECT_NE(&appSettingsStore(nextAppName()), &appSettingsStore(nextAppName()));
}

TEST_F(AppSettingsTest, StoreLoadsExistingSettingsFile)
{
    const auto appName = nextAppName();
    writeText(m_directory / "config" / appName / "settings.json", R"({"version": 1, "settings": {"locale": "de"}})");

    EXPECT_EQ(appSettingsStore(appName).get<std::string>("locale", "en"), "de");
}

TEST_F(AppSettingsTest, LegacyValueIsImportedAndPersisted)
{
    const auto legacyFile = m_directory / "data" / "app" / "locale.txt";
    writeText(legacyFile, "pl\n");

    SettingsStore store(m_directory / "config" / "settings.json");

    EXPECT_TRUE(importLegacyTextSetting(store, "locale", legacyFile));
    EXPECT_EQ(store.get<std::string>("locale", ""), "pl");

    SettingsStore reader(store.filePath());
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<std::string>("locale", ""), "pl");
}

TEST_F(AppSettingsTest, LegacyImportAppliesNormalizer)
{
    const auto legacyFile = m_directory / "locale.txt";
    writeText(legacyFile, "PL");

    SettingsStore store(m_directory / "settings.json");
    const auto toLower = [](const std::string& value)
    {
        std::string result = value;
        for (auto& character : result)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return result;
    };

    EXPECT_TRUE(importLegacyTextSetting(store, "locale", legacyFile, toLower));
    EXPECT_EQ(store.get<std::string>("locale", ""), "pl");
}

TEST_F(AppSettingsTest, LegacyImportDoesNotOverrideExistingKey)
{
    const auto legacyFile = m_directory / "locale.txt";
    writeText(legacyFile, "pl");

    SettingsStore store(m_directory / "settings.json");
    store.set<std::string>("locale", "de");

    EXPECT_FALSE(importLegacyTextSetting(store, "locale", legacyFile));
    EXPECT_EQ(store.get<std::string>("locale", ""), "de");
}

TEST_F(AppSettingsTest, LegacyImportIgnoresMissingFile)
{
    SettingsStore store(m_directory / "settings.json");

    EXPECT_FALSE(importLegacyTextSetting(store, "locale", m_directory / "nope.txt"));
    EXPECT_FALSE(store.contains("locale"));
}

TEST_F(AppSettingsTest, LegacyImportIgnoresEmptyFile)
{
    const auto legacyFile = m_directory / "locale.txt";
    writeText(legacyFile, "");

    SettingsStore store(m_directory / "settings.json");

    EXPECT_FALSE(importLegacyTextSetting(store, "locale", legacyFile));
    EXPECT_FALSE(store.contains("locale"));
}

TEST_F(AppSettingsTest, LegacyImportLeavesTheOldFileInPlace)
{
    const auto legacyFile = m_directory / "locale.txt";
    writeText(legacyFile, "pl");

    SettingsStore store(m_directory / "settings.json");
    ASSERT_TRUE(importLegacyTextSetting(store, "locale", legacyFile));

    EXPECT_TRUE(fs::exists(legacyFile));
}
