#pragma once

#include "stapik/cloud/CloudStorageConfig.hpp"

#include <optional>
#include <string>

class CloudStorageConfigStorage
{
public:
    static bool save(const CloudStorageConfig& config, const std::string& appName);
    static std::optional<CloudStorageConfig> load(const std::string& appName);
};