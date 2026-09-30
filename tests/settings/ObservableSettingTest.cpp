#include "stapik/settings/ObservableSetting.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::LoadStatus;
    using stapik::settings::ObservableSetting;
    using stapik::settings::SettingsStore;

    class ObservableSettingTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-observable-XXXXXX").string();
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

TEST_F(ObservableSettingTest, StartsWithDefaultWhenStoreHasNoValue)
{
    SettingsStore store(m_path);
    const ObservableSetting<std::string> setting(store, "theme", "classic");

    EXPECT_EQ(setting.get(), "classic");
}

TEST_F(ObservableSettingTest, StartsWithStoredValue)
{
    SettingsStore writer(m_path);
    writer.set<std::string>("theme", "modern");
    ASSERT_TRUE(writer.save());

    SettingsStore store(m_path);
    ASSERT_EQ(store.load(), LoadStatus::Loaded);
    ObservableSetting<std::string> setting(store, "theme", "classic");

    EXPECT_EQ(setting.get(), "modern");
}

TEST_F(ObservableSettingTest, SetPersistsAndEmitsChangedValue)
{
    SettingsStore store(m_path);
    ObservableSetting<std::string> setting(store, "theme", "classic");

    std::vector<std::string> emitted;
    setting.signalChanged().connect([&emitted](const std::string& value) { emitted.push_back(value); });

    EXPECT_TRUE(setting.set("modern"));

    EXPECT_EQ(setting.get(), "modern");
    ASSERT_EQ(emitted.size(), 1u);
    EXPECT_EQ(emitted.front(), "modern");

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<std::string>("theme", ""), "modern");
}

TEST_F(ObservableSettingTest, SettingTheSameValueDoesNothing)
{
    SettingsStore store(m_path);
    ObservableSetting setting(store, "fontSize", 12);

    int emissions = 0;
    setting.signalChanged().connect([&emissions](const int&) { ++emissions; });

    EXPECT_FALSE(setting.set(12));

    EXPECT_EQ(emissions, 0);
    EXPECT_FALSE(fs::exists(m_path));
}

TEST_F(ObservableSettingTest, ResetRestoresDefault)
{
    SettingsStore store(m_path);
    ObservableSetting setting(store, "fontSize", 12);
    setting.set(20);

    EXPECT_TRUE(setting.reset());

    EXPECT_EQ(setting.get(), 12);
}

TEST_F(ObservableSettingTest, ReloadPicksUpChangedStoreValueAndEmits)
{
    SettingsStore store(m_path);
    ObservableSetting setting(store, "fontSize", 12);

    int lastEmitted = 0;
    setting.signalChanged().connect([&lastEmitted](const int& value) { lastEmitted = value; });

    store.set<int>("fontSize", 18);
    setting.reload();

    EXPECT_EQ(setting.get(), 18);
    EXPECT_EQ(lastEmitted, 18);
}

TEST_F(ObservableSettingTest, ReloadWithoutChangeDoesNotEmit)
{
    SettingsStore store(m_path);
    ObservableSetting setting(store, "fontSize", 12);

    int emissions = 0;
    setting.signalChanged().connect([&emissions](const int&) { ++emissions; });

    setting.reload();

    EXPECT_EQ(emissions, 0);
}

TEST_F(ObservableSettingTest, TwoSettingsShareOneStore)
{
    SettingsStore store(m_path);
    ObservableSetting<std::string> locale(store, "locale", "en");
    ObservableSetting<std::string> theme(store, "theme", "classic");

    locale.set("pl");
    theme.set("modern");

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.get<std::string>("locale", ""), "pl");
    EXPECT_EQ(reader.get<std::string>("theme", ""), "modern");
}
