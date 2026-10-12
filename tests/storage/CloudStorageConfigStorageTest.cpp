#include "stapik/storage/CloudStorageConfigStorage.hpp"

#include "support/FakeSecretStore.hpp"
#include "support/ScopedEnvironment.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    constexpr auto APP_NAME = "app";
    constexpr auto API_KEY_ACCOUNT = "cloud-api-key";

    class CloudStorageConfigStorageTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-cloudconfig-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;

            setenv("XDG_DATA_HOME", (m_directory / "data").c_str(), 1);
            setenv("XDG_CONFIG_HOME", (m_directory / "config").c_str(), 1);
            fs::create_directories(configPath().parent_path());
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        [[nodiscard]] fs::path configPath() const
        {
            return m_directory / "config" / APP_NAME / "config.json";
        }

        [[nodiscard]] fs::path legacyConfigPath() const
        {
            return m_directory / "data" / APP_NAME / "config.json";
        }

        [[nodiscard]] std::string configText() const
        {
            std::ifstream file(configPath());
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        void writeConfigText(const std::string& text) const
        {
            std::ofstream(configPath()) << text;
        }

        void writeLegacyConfigText(const std::string& text) const
        {
            fs::create_directories(legacyConfigPath().parent_path());
            std::ofstream(legacyConfigPath()) << text;
        }

        stapik::test::ScopedEnvironment m_dataHome{ "XDG_DATA_HOME" };
        stapik::test::ScopedEnvironment m_configHome{ "XDG_CONFIG_HOME" };
        stapik::test::FakeSecretStore m_secretStore;
        fs::path m_directory;
    };
}

TEST_F(CloudStorageConfigStorageTest, SavedApiKeyGoesToSecretStoreNotToFile)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));

    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT), "secret-key");
    EXPECT_EQ(configText().find("secret-key"), std::string::npos);
    EXPECT_NE(configText().find("https://cloud.example"), std::string::npos);
}

TEST_F(CloudStorageConfigStorageTest, LoadReturnsApiKeyFromSecretStore)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiUrl, "https://cloud.example");
    EXPECT_EQ(config->apiKey, "secret-key");
}

TEST_F(CloudStorageConfigStorageTest, ApiKeyStaysInFileWhenSecretStoreIsUnavailable)
{
    m_secretStore.available = false;

    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));
    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    EXPECT_NE(configText().find("secret-key"), std::string::npos);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiKey, "secret-key");
}

TEST_F(CloudStorageConfigStorageTest, PlainTextApiKeyMovesToSecretStoreOnLoad)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example","apiKey":"old-key"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiKey, "old-key");
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT), "old-key");
    EXPECT_EQ(configText().find("old-key"), std::string::npos);
}

TEST_F(CloudStorageConfigStorageTest, PlainTextApiKeyIsKeptWhenSecretStoreIsUnavailable)
{
    m_secretStore.available = false;
    writeConfigText(R"({"apiUrl":"https://cloud.example","apiKey":"old-key"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiKey, "old-key");
    EXPECT_NE(configText().find("old-key"), std::string::npos);
}

TEST_F(CloudStorageConfigStorageTest, SavingEmptyApiKeyRemovesItFromSecretStore)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));

    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "" }, APP_NAME, m_secretStore));
    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    EXPECT_FALSE(m_secretStore.contains(APP_NAME, API_KEY_ACCOUNT));
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->apiKey.empty());
}

TEST_F(CloudStorageConfigStorageTest, LoadWithoutStoredKeyReturnsEmptyApiKey)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiUrl, "https://cloud.example");
    EXPECT_TRUE(config->apiKey.empty());
}

TEST_F(CloudStorageConfigStorageTest, LoadWithoutConfigFileReturnsNothing)
{
    EXPECT_FALSE(CloudStorageConfigStorage::load(APP_NAME, m_secretStore).has_value());
}

TEST_F(CloudStorageConfigStorageTest, LoadOfBrokenConfigFileReturnsNothing)
{
    writeConfigText("{ not json");

    EXPECT_FALSE(CloudStorageConfigStorage::load(APP_NAME, m_secretStore).has_value());
}

TEST_F(CloudStorageConfigStorageTest, KeysOfDifferentApplicationsAreSeparate)
{
    ASSERT_TRUE(m_secretStore.store("other-app", API_KEY_ACCOUNT, "other-key"));
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));

    EXPECT_EQ(m_secretStore.lookup("other-app", API_KEY_ACCOUNT), "other-key");
}

TEST_F(CloudStorageConfigStorageTest, SaveWritesToConfigDirectoryOnly)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));

    EXPECT_TRUE(fs::exists(configPath()));
    EXPECT_FALSE(fs::exists(legacyConfigPath()));
}

TEST_F(CloudStorageConfigStorageTest, LoadMovesConfigFromDataDirectory)
{
    writeLegacyConfigText(R"({"apiUrl":"https://cloud.example"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiUrl, "https://cloud.example");
    EXPECT_NE(configText().find("https://cloud.example"), std::string::npos);
    EXPECT_FALSE(fs::exists(legacyConfigPath()));
}

TEST_F(CloudStorageConfigStorageTest, MigratedPlainTextApiKeyMovesToSecretStore)
{
    writeLegacyConfigText(R"({"apiUrl":"https://cloud.example","apiKey":"old-key"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiKey, "old-key");
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT), "old-key");
    EXPECT_EQ(configText().find("old-key"), std::string::npos);
    EXPECT_FALSE(fs::exists(legacyConfigPath()));
}

TEST_F(CloudStorageConfigStorageTest, ConfigDirectoryWinsOverLegacyFile)
{
    writeConfigText(R"({"apiUrl":"https://new.example"})");
    writeLegacyConfigText(R"({"apiUrl":"https://old.example","apiKey":"old-key"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->apiUrl, "https://new.example");
    EXPECT_FALSE(fs::exists(legacyConfigPath()));
}

TEST_F(CloudStorageConfigStorageTest, SaveRemovesLegacyFile)
{
    writeLegacyConfigText(R"({"apiUrl":"https://old.example","apiKey":"old-key"})");

    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "" }, APP_NAME, m_secretStore));

    EXPECT_FALSE(fs::exists(legacyConfigPath()));
    EXPECT_NE(configText().find("https://cloud.example"), std::string::npos);
}
