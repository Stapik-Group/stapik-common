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
    constexpr auto PASSPHRASE_ACCOUNT = "cloud-encryption-passphrase";

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

    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT).secret, "secret-key");
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
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT).secret, "old-key");
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

    EXPECT_EQ(m_secretStore.lookup("other-app", API_KEY_ACCOUNT).secret, "other-key");
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
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT).secret, "old-key");
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

TEST_F(CloudStorageConfigStorageTest, SavedPassphraseGoesToSecretStoreNotToFile)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "secret-key", .encryptionPassphrase = "secret-passphrase" };

    ASSERT_TRUE(CloudStorageConfigStorage::save(config, APP_NAME, m_secretStore));

    EXPECT_EQ(m_secretStore.lookup(APP_NAME, PASSPHRASE_ACCOUNT).secret, "secret-passphrase");
    EXPECT_EQ(configText().find("secret-passphrase"), std::string::npos);
}

TEST_F(CloudStorageConfigStorageTest, LoadReturnsPassphraseFromSecretStore)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "secret-key", .encryptionPassphrase = "secret-passphrase" };
    ASSERT_TRUE(CloudStorageConfigStorage::save(config, APP_NAME, m_secretStore));

    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->apiKey, "secret-key");
    EXPECT_EQ(loaded->encryptionPassphrase, "secret-passphrase");
}

TEST_F(CloudStorageConfigStorageTest, PassphraseStaysInFileWhenSecretStoreIsUnavailable)
{
    m_secretStore.available = false;
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "secret-key", .encryptionPassphrase = "secret-passphrase" };

    ASSERT_TRUE(CloudStorageConfigStorage::save(config, APP_NAME, m_secretStore));
    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    EXPECT_NE(configText().find("secret-passphrase"), std::string::npos);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->encryptionPassphrase, "secret-passphrase");
}

TEST_F(CloudStorageConfigStorageTest, PlainTextSecretsBothMoveToSecretStoreOnLoad)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example","apiKey":"old-key","encryptionPassphrase":"old-passphrase"})");

    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->apiKey, "old-key");
    EXPECT_EQ(loaded->encryptionPassphrase, "old-passphrase");
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT).secret, "old-key");
    EXPECT_EQ(m_secretStore.lookup(APP_NAME, PASSPHRASE_ACCOUNT).secret, "old-passphrase");
    EXPECT_EQ(configText().find("old-key"), std::string::npos);
    EXPECT_EQ(configText().find("old-passphrase"), std::string::npos);
}

TEST_F(CloudStorageConfigStorageTest, SavingEmptyPassphraseRemovesItFromSecretStore)
{
    const CloudStorageConfig encrypted{ .apiUrl = "https://cloud.example", .apiKey = "secret-key", .encryptionPassphrase = "secret-passphrase" };
    ASSERT_TRUE(CloudStorageConfigStorage::save(encrypted, APP_NAME, m_secretStore));

    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));
    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    EXPECT_FALSE(m_secretStore.contains(APP_NAME, PASSPHRASE_ACCOUNT));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->encryptionPassphrase.empty());
}

TEST_F(CloudStorageConfigStorageTest, LoadWithoutStoredPassphraseReturnsEmptyPassphrase)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example"})");

    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->isEndToEndEncrypted());
}

TEST_F(CloudStorageConfigStorageTest, SaveReplacesInvalidUtf8InsteadOfFailing)
{
    m_secretStore.available = false;

    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "bad\xffkey" }, APP_NAME, m_secretStore));
    const auto loaded = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->apiUrl, "https://cloud.example");
    EXPECT_EQ(loaded->apiKey, "bad\xEF\xBF\xBD" "key");
}

TEST_F(CloudStorageConfigStorageTest, LoadReportsUnreadableSecretStoreInsteadOfAnEmptyKey)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key", "secret-passphrase" }, APP_NAME, m_secretStore));
    m_secretStore.lookupFails = true;

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->secretsUnreadable);
    EXPECT_TRUE(config->apiKey.empty());
    EXPECT_TRUE(config->encryptionPassphrase.empty());
}

TEST_F(CloudStorageConfigStorageTest, SecretMissingFromSecretStoreIsNotReportedAsUnreadable)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example"})");

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->secretsUnreadable);
}

TEST_F(CloudStorageConfigStorageTest, SecretsInConfigFileAreNotReportedAsUnreadable)
{
    writeConfigText(R"({"apiUrl":"https://cloud.example","apiKey":"old-key","encryptionPassphrase":"old-passphrase"})");
    m_secretStore.lookupFails = true;

    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->secretsUnreadable);
    EXPECT_EQ(config->apiKey, "old-key");
    EXPECT_EQ(config->encryptionPassphrase, "old-passphrase");
}

TEST_F(CloudStorageConfigStorageTest, SavingAfterAnUnreadableLoadKeepsTheStoredSecrets)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key", "secret-passphrase" }, APP_NAME, m_secretStore));
    m_secretStore.lookupFails = true;
    const auto unreadable = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);
    ASSERT_TRUE(unreadable.has_value());
    m_secretStore.lookupFails = false;

    ASSERT_TRUE(CloudStorageConfigStorage::save(*unreadable, APP_NAME, m_secretStore));
    const auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);

    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(config->secretsUnreadable);
    EXPECT_EQ(config->apiKey, "secret-key");
    EXPECT_EQ(config->encryptionPassphrase, "secret-passphrase");
}

TEST_F(CloudStorageConfigStorageTest, NewKeyReplacesTheStoredOneAfterAnUnreadableLoad)
{
    ASSERT_TRUE(CloudStorageConfigStorage::save({ "https://cloud.example", "secret-key" }, APP_NAME, m_secretStore));
    m_secretStore.lookupFails = true;
    auto config = CloudStorageConfigStorage::load(APP_NAME, m_secretStore);
    ASSERT_TRUE(config.has_value());
    m_secretStore.lookupFails = false;
    config->apiKey = "new-key";

    ASSERT_TRUE(CloudStorageConfigStorage::save(*config, APP_NAME, m_secretStore));

    EXPECT_EQ(m_secretStore.lookup(APP_NAME, API_KEY_ACCOUNT).secret, "new-key");
}
