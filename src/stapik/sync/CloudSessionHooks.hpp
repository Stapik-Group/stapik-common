#pragma once

#include "stapik/cloud/ApiKeyExpiry.hpp"
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
        std::function<void()> clearConfig;
        std::function<std::unique_ptr<ICloudStorage>(const CloudStorageConfig&)> createStorage;
        // Filled by the storages createStorage makes, read by the session to warn about an expiring API key.
        std::shared_ptr<cloud::ApiKeyExpiryMonitor> expiryMonitor;
    };

    [[nodiscard]] CloudSessionHooks defaultCloudSessionHooks(std::string slotKey);
}
