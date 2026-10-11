#pragma once

#include "stapik/cloud/CloudStorageConfig.hpp"
#include "stapik/storage/SecretStore.hpp"

#include <optional>
#include <string>

class CloudStorageConfigStorage
{
public:
    static bool save(const CloudStorageConfig& config, const std::string& appName);
    static std::optional<CloudStorageConfig> load(const std::string& appName);

    static bool save(const CloudStorageConfig& config, const std::string& appName, stapik::storage::ISecretStore& secretStore);
    static std::optional<CloudStorageConfig> load(const std::string& appName, stapik::storage::ISecretStore& secretStore);
};
