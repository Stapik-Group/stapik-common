#include "CloudSessionHooks.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/cloud/CloudStorageFactory.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/storage/CloudStorageConfigStorage.hpp"

#include <memory>
#include <utility>

namespace stapik::sync
{
    CloudSessionHooks defaultCloudSessionHooks(std::string slotKey)
    {
        const auto appName = app::AppContext::instance().info().internalName;

        CloudSessionHooks hooks;
        hooks.expiryMonitor = std::make_shared<cloud::ApiKeyExpiryMonitor>();

        hooks.loadConfig = [appName]
        {
            return CloudStorageConfigStorage::load(appName);
        };

        hooks.saveConfig = [appName](const CloudStorageConfig& config)
        {
            return CloudStorageConfigStorage::save(config, appName).hasValue();
        };

        hooks.clearConfig = [appName]
        {
            if (const auto result = CloudStorageConfigStorage::clear(appName); !result)
                log::warning("Cannot remove the saved cloud configuration: {}", result.error().message());
        };

        hooks.createStorage = [slotKey = std::move(slotKey), monitor = hooks.expiryMonitor](const CloudStorageConfig& config) -> std::unique_ptr<ICloudStorage>
        {
            return cloud::createCloudStorage(config, slotKey, http::defaultHttpClient(), monitor);
        };

        return hooks;
    }
}
