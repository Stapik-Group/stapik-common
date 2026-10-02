#pragma once

#include "stapik/cloud/CloudStorageConfig.hpp"
#include "stapik/cloud/ICloudStorage.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace stapik::sync
{
    struct CloudSessionHooks
    {
        std::function<std::optional<CloudStorageConfig>()> loadConfig;
        std::function<bool(const CloudStorageConfig&)> saveConfig;
        std::function<std::unique_ptr<ICloudStorage>(const CloudStorageConfig&)> createStorage;
    };

    [[nodiscard]] CloudSessionHooks defaultCloudSessionHooks(std::string slotKey);
}
