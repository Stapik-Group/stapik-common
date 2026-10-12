#pragma once

#include "SettingsStore.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace stapik::settings
{
    [[nodiscard]] SettingsStore& appSettingsStore(const std::string& appName);
    // Also sets aside the legacy text files, which would otherwise be imported again on the next start.
    // Fails with the first problem met; the remaining files are still set aside.
    storage::FileResult resetAppSettings(const std::string& appName);
    bool importLegacyTextSetting(
        SettingsStore& store,
        const std::string& key,
        const std::filesystem::path& legacyFile,
        const std::function<std::string(const std::string&)>& normalize = {});
}
