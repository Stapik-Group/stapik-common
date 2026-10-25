#pragma once

#include "stapik/cloud/CloudStorageConfig.hpp"
#include "stapik/storage/FileError.hpp"
#include "stapik/storage/SecretStore.hpp"

#include <optional>
#include <string>

struct CloudSecretsInConfigFile
{
    bool apiKey = false;
    bool encryptionPassphrase = false;

    [[nodiscard]] bool any() const
    {
        return apiKey || encryptionPassphrase;
    }
};

class CloudStorageConfigStorage
{
public:
    static stapik::storage::FileResult save(const CloudStorageConfig& config, const std::string& appName);
    static std::optional<CloudStorageConfig> load(const std::string& appName);

    static CloudSecretsInConfigFile secretsKeptInConfigFile(const std::string& appName);

    static stapik::storage::FileResult save(const CloudStorageConfig& config, const std::string& appName, stapik::storage::ISecretStore& secretStore);
    static std::optional<CloudStorageConfig> load(const std::string& appName, stapik::storage::ISecretStore& secretStore);
};
