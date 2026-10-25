#include "CloudStorageConfigStorage.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/text/JsonText.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <glib.h>
#include <optional>
#include <sstream>
#include <utility>

namespace
{
    constexpr auto API_KEY_ACCOUNT = "cloud-api-key";
    constexpr auto ENCRYPTION_PASSPHRASE_ACCOUNT = "cloud-encryption-passphrase";

    struct FileSecrets
    {
        std::string apiKey;
        std::string encryptionPassphrase;

        bool operator==(const FileSecrets&) const = default;
    };

    std::filesystem::path configPath(const std::string& appName)
    {
        return AppPaths::userConfigDir(appName) / "config.json";
    }

    std::filesystem::path legacyConfigPath(const std::string& appName)
    {
        return AppPaths::userDataDir(appName) / "config.json";
    }

    // The file used to live in the data directory. The copy in the config directory is the authoritative one,
    // so when both exist the old file is only a leftover (possibly with a plain text API key) and is removed.
    void migrateLegacyConfig(const std::string& appName)
    {
        const auto legacyPath = legacyConfigPath(appName);
        const auto path = configPath(appName);

        std::error_code errorCode;
        if (!std::filesystem::exists(legacyPath, errorCode))
            return;

        if (!std::filesystem::exists(path, errorCode))
        {
            std::ifstream legacyFile(legacyPath, std::ios::binary);
            std::stringstream content;
            content << legacyFile.rdbuf();
            if (!legacyFile.is_open() || legacyFile.bad())
            {
                stapik::log::warning("Cannot read {}, leaving it where it is", stapik::storage::pathText(legacyPath));
                return;
            }

            if (!stapik::storage::writeFileAtomically(path, content.str(), stapik::storage::OWNER_READ_WRITE))
                return;
        }

        std::filesystem::remove(legacyPath, errorCode);
        if (errorCode)
            stapik::log::warning("Cannot remove {}: {}", stapik::storage::pathText(legacyPath), errorCode.message());
        else
            stapik::log::info("Moved the cloud config from {} to {}", stapik::storage::pathText(legacyPath), stapik::storage::pathText(path));
    }

    void restrictToOwner(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::permissions(
            path,
            stapik::storage::OWNER_READ_WRITE,
            std::filesystem::perm_options::replace,
            errorCode);

        if (errorCode)
            stapik::log::warning("Cannot restrict permissions of {}: {}", stapik::storage::pathText(path), errorCode.message());
    }

    stapik::storage::FileResult writeConfigFile(const std::filesystem::path& path, const std::string& apiUrl, const FileSecrets& secrets)
    {
        nlohmann::json json = { { "apiUrl", apiUrl } };
        if (!secrets.apiKey.empty())
            json["apiKey"] = secrets.apiKey;
        if (!secrets.encryptionPassphrase.empty())
            json["encryptionPassphrase"] = secrets.encryptionPassphrase;

        return stapik::storage::writeFileAtomically(path, stapik::text::dumpJson(json, 2), stapik::storage::OWNER_READ_WRITE);
    }

    struct ConfigFileContent
    {
        std::string apiUrl;
        FileSecrets secrets;
    };

    std::optional<ConfigFileContent> readConfigFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            stapik::log::warning("Cannot open the cloud config {}", stapik::storage::pathText(path));
            return std::nullopt;
        }

        try
        {
            const auto json = nlohmann::json::parse(file);
            return ConfigFileContent{
                .apiUrl = json.at("apiUrl").get<std::string>(),
                .secrets = {
                    .apiKey = json.value("apiKey", std::string{}),
                    .encryptionPassphrase = json.value("encryptionPassphrase", std::string{})
                }
            };
        }
        catch (const nlohmann::json::exception& exception)
        {
            stapik::log::warning("The cloud config {} cannot be used: {}", stapik::storage::pathText(path), exception.what());
            return std::nullopt;
        }
    }

    struct ResolvedSecret
    {
        std::string value;
        bool unreadable = false;
    };

    ResolvedSecret resolveSecret(
        stapik::storage::ISecretStore& secretStore,
        const std::string& appName,
        const char* account,
        const std::string& secretFromFile)
    {
        if (!secretFromFile.empty())
            return { secretFromFile, false };

        const auto lookup = secretStore.lookup(appName, account);
        return { lookup.secret, lookup.isFailed() };
    }

    std::string secretToKeepInFile(
        stapik::storage::ISecretStore& secretStore,
        const std::string& appName,
        const char* account,
        const std::string& secret,
        const char* description,
        const bool secretStoreWasUnreadable)
    {
        if (secret.empty())
        {
            if (!secretStoreWasUnreadable)
                secretStore.remove(appName, account);
            return {};
        }

        if (secretStore.store(appName, account, secret))
            return {};

        stapik::log::warning("No secret store available, the {} is kept in the config file", description);
        return secret;
    }

    std::string secretLeftInFile(
        stapik::storage::ISecretStore& secretStore,
        const std::string& appName,
        const char* account,
        const std::string& secretFromFile)
    {
        if (secretFromFile.empty() || !secretStore.store(appName, account, secretFromFile))
            return secretFromFile;

        return {};
    }

    void moveFileSecretsToSecretStore(
        const std::filesystem::path& path,
        const std::string& apiUrl,
        const FileSecrets& secretsInFile,
        const std::string& appName,
        stapik::storage::ISecretStore& secretStore)
    {
        const FileSecrets remaining{
            .apiKey = secretLeftInFile(secretStore, appName, API_KEY_ACCOUNT, secretsInFile.apiKey),
            .encryptionPassphrase = secretLeftInFile(secretStore, appName, ENCRYPTION_PASSPHRASE_ACCOUNT, secretsInFile.encryptionPassphrase)
        };

        if (remaining == secretsInFile)
            return;

        if (writeConfigFile(path, apiUrl, remaining))
            stapik::log::info("Moved the cloud secrets from {} to the secret store", stapik::storage::pathText(path));
        else
            stapik::log::warning("Cannot remove the cloud secrets from {}", stapik::storage::pathText(path));
    }
}

stapik::storage::FileResult CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName)
{
    return save(config, appName, stapik::storage::platformSecretStore());
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName)
{
    return load(appName, stapik::storage::platformSecretStore());
}

stapik::storage::FileResult CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName, stapik::storage::ISecretStore& secretStore)
{
    migrateLegacyConfig(appName);

    const FileSecrets secretsInFile{
        .apiKey = secretToKeepInFile(secretStore, appName, API_KEY_ACCOUNT, config.apiKey, "cloud API key", config.secretsUnreadable),
        .encryptionPassphrase = secretToKeepInFile(secretStore, appName, ENCRYPTION_PASSPHRASE_ACCOUNT, config.encryptionPassphrase, "cloud encryption passphrase", config.secretsUnreadable)
    };

    return writeConfigFile(configPath(appName), config.apiUrl, secretsInFile);
}

CloudSecretsInConfigFile CloudStorageConfigStorage::secretsKeptInConfigFile(const std::string& appName)
{
    const auto path = configPath(appName);
    std::error_code errorCode;
    if (!std::filesystem::exists(path, errorCode))
        return {};

    const auto content = readConfigFile(path);
    if (!content)
        return {};

    return { .apiKey = !content->secrets.apiKey.empty(), .encryptionPassphrase = !content->secrets.encryptionPassphrase.empty() };
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName, stapik::storage::ISecretStore& secretStore)
{
    migrateLegacyConfig(appName);

    const auto path = configPath(appName);
    std::error_code errorCode;
    const bool configExists = std::filesystem::exists(path, errorCode);
    if (errorCode)
        stapik::log::warning("Cannot check {}: {}", stapik::storage::pathText(path), errorCode.message());

    if (!configExists)
        return std::nullopt;

    restrictToOwner(path);

    const auto content = readConfigFile(path);
    if (!content)
        return std::nullopt;

    CloudStorageConfig config;
    config.apiUrl = content->apiUrl;
    const auto& secretsInFile = content->secrets;

    moveFileSecretsToSecretStore(path, config.apiUrl, secretsInFile, appName, secretStore);

    auto apiKey = resolveSecret(secretStore, appName, API_KEY_ACCOUNT, secretsInFile.apiKey);
    auto encryptionPassphrase = resolveSecret(secretStore, appName, ENCRYPTION_PASSPHRASE_ACCOUNT, secretsInFile.encryptionPassphrase);

    config.apiKey = std::move(apiKey.value);
    config.encryptionPassphrase = std::move(encryptionPassphrase.value);
    config.secretsUnreadable = apiKey.unreadable || encryptionPassphrase.unreadable;

    return config;
}
