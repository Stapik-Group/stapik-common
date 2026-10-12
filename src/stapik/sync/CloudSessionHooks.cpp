#include "CloudSessionHooks.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/cloud/CloudStorageFactory.hpp"
#include "stapik/storage/CloudStorageConfigStorage.hpp"

#include <utility>

namespace stapik::sync
{
    CloudSessionHooks defaultCloudSessionHooks(std::string slotKey)
    {
        const auto appName = app::AppContext::instance().info().internalName;

        CloudSessionHooks hooks;

        hooks.loadConfig = [appName]
        {
            return CloudStorageConfigStorage::load(appName);
        };

        hooks.saveConfig = [appName](const CloudStorageConfig& config)
        {
            return CloudStorageConfigStorage::save(config, appName).hasValue();
        };

        hooks.createStorage = [slotKey = std::move(slotKey)](const CloudStorageConfig& config) -> std::unique_ptr<ICloudStorage>
        {
            return cloud::createCloudStorage(config, slotKey);
        };

        return hooks;
    }
}
