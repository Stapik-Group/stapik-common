#include "AppSettings.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"

#include <fstream>
#include <map>
#include <memory>
#include <mutex>

namespace stapik::settings
{
    SettingsStore& appSettingsStore(const std::string& appName)
    {
        static std::mutex mutex;
        static std::map<std::string, std::unique_ptr<SettingsStore>> stores;

        const std::lock_guard lock(mutex);

        auto& store = stores[appName];
        if (!store)
        {
            store = std::make_unique<SettingsStore>(AppPaths::userConfigDir(appName) / "settings.json");
            store->load();
        }

        return *store;
    }

    bool importLegacyTextSetting(
        SettingsStore& store,
        const std::string& key,
        const std::filesystem::path& legacyFile,
        const std::function<std::string(const std::string&)>& normalize)
    {
        if (store.contains(key))
            return false;

        if (std::error_code errorCode; !std::filesystem::exists(legacyFile, errorCode))
            return false;

        std::ifstream file(legacyFile);
        std::string content;
        if (!file.is_open() || !(file >> content))
            return false;

        store.set<std::string>(key, normalize ? normalize(content) : content);

        if (store.save())
            log::info("Imported legacy setting '{}' from {}", key, storage::pathText(legacyFile));
        else
            log::warning("Cannot persist setting '{}' imported from {}", key, storage::pathText(legacyFile));

        return true;
    }
}
