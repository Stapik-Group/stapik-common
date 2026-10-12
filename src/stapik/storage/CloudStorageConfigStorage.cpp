#include "CloudStorageConfigStorage.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <glib.h>

namespace
{
    constexpr auto API_KEY_ACCOUNT = "cloud-api-key";

    std::filesystem::path configPath(const std::string& appName)
    {
        return AppPaths::userDataDir(appName) / "config.json";
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

    bool writeConfigFile(const std::filesystem::path& path, const std::string& apiUrl, const std::string& plainTextApiKey)
    {
        nlohmann::json json = { { "apiUrl", apiUrl } };
        if (!plainTextApiKey.empty())
            json["apiKey"] = plainTextApiKey;

        return stapik::storage::writeFileAtomically(path, json.dump(2), stapik::storage::OWNER_READ_WRITE).hasValue();
    }

    void moveLegacyApiKeyToSecretStore(
        const std::filesystem::path& path,
        const CloudStorageConfig& config,
        const std::string& appName,
        stapik::storage::ISecretStore& secretStore)
    {
        if (!secretStore.store(appName, API_KEY_ACCOUNT, config.apiKey))
            return;

        if (writeConfigFile(path, config.apiUrl, {}))
            stapik::log::info("Moved the cloud API key from {} to the secret store", stapik::storage::pathText(path));
        else
            stapik::log::warning("Cannot remove the cloud API key from {}", stapik::storage::pathText(path));
    }
}

bool CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName)
{
    return save(config, appName, stapik::storage::platformSecretStore());
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName)
{
    return load(appName, stapik::storage::platformSecretStore());
}

bool CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName, stapik::storage::ISecretStore& secretStore)
{
    std::string plainTextApiKey;

    if (config.apiKey.empty())
    {
        secretStore.remove(appName, API_KEY_ACCOUNT);
    }
    else if (!secretStore.store(appName, API_KEY_ACCOUNT, config.apiKey))
    {
        stapik::log::warning("No secret store available, the cloud API key is kept in the config file");
        plainTextApiKey = config.apiKey;
    }

    return writeConfigFile(configPath(appName), config.apiUrl, plainTextApiKey);
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName, stapik::storage::ISecretStore& secretStore)
{
    const auto path = configPath(appName);
    if (!std::filesystem::exists(path))
        return std::nullopt;

    restrictToOwner(path);

    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    CloudStorageConfig config;
    try
    {
        const auto json = nlohmann::json::parse(file);
        config.apiUrl = json.at("apiUrl").get<std::string>();
        config.apiKey = json.value("apiKey", std::string{});
    }
    catch (const nlohmann::json::exception&)
    {
        return std::nullopt;
    }

    if (config.apiKey.empty())
        config.apiKey = secretStore.lookup(appName, API_KEY_ACCOUNT).value_or(std::string{});
    else
        moveLegacyApiKeyToSecretStore(path, config, appName, secretStore);

    return config;
}
