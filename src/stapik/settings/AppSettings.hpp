#pragma once

#include "SettingsStore.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace stapik::settings
{
    [[nodiscard]] SettingsStore& appSettingsStore(const std::string& appName);
    bool importLegacyTextSetting(
        SettingsStore& store,
        const std::string& key,
        const std::filesystem::path& legacyFile,
        const std::function<std::string(const std::string&)>& normalize = {});
}
