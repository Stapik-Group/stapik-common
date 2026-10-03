#include "stapik/locale/LocaleJson.hpp"
#include "stapik/settings/ObservableSetting.hpp"
#include "stapik/theme/ThemeJson.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::LoadStatus;
    using stapik::settings::ObservableSetting;
    using stapik::settings::SettingsStore;

    std::string readText(const fs::path& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    class EnumSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-enum-settings-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
            m_path = m_directory / "settings.json";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
        fs::path m_path;
    };
}

TEST_F(EnumSettingsTest, LocaleIsStoredAsFileString)
{
    SettingsStore writer(m_path);
    writer.set<Locale>("locale", Locale::PL);
    ASSERT_TRUE(writer.save());

    EXPECT_NE(readText(m_path).find("\"pl\""), std::string::npos);

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<Locale>("locale", Locale::EN), Locale::PL);
}

TEST_F(EnumSettingsTest, UnknownLocaleStringMapsToEnglish)
{
    SettingsStore store(m_path);
    store.set<std::string>("locale", "xx");

    EXPECT_EQ(store.get<Locale>("locale", Locale::DE), Locale::EN);
}

TEST_F(EnumSettingsTest, NonStringLocaleFallsBackToDefault)
{
    SettingsStore store(m_path);
    store.set<int>("locale", 5);

    EXPECT_EQ(store.get<Locale>("locale", Locale::DE), Locale::DE);
}

TEST_F(EnumSettingsTest, ThemeIsStoredAsFileString)
{
    SettingsStore writer(m_path);
    writer.set<Theme>("theme", Theme::ClassicPink);
    ASSERT_TRUE(writer.save());

    EXPECT_NE(readText(m_path).find("\"classic-pink\""), std::string::npos);

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<Theme>("theme", Theme::Classic), Theme::ClassicPink);
}

TEST_F(EnumSettingsTest, UnknownThemeStringMapsToClassic)
{
    SettingsStore store(m_path);
    store.set<std::string>("theme", "neon");

    EXPECT_EQ(store.get<Theme>("theme", Theme::Modern), Theme::Classic);
}

TEST_F(EnumSettingsTest, ObservableThemeSettingEmitsAndPersists)
{
    SettingsStore store(m_path);
    ObservableSetting<Theme> setting(store, "theme", Theme::Classic);

    std::vector<Theme> emitted;
    setting.signalChanged().connect([&emitted](const Theme& theme) { emitted.push_back(theme); });

    EXPECT_TRUE(setting.set(Theme::Modern));

    ASSERT_EQ(emitted.size(), 1u);
    EXPECT_EQ(emitted.front(), Theme::Modern);

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<Theme>("theme", Theme::Classic), Theme::Modern);
}
