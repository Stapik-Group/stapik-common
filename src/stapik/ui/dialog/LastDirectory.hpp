#pragma once

#include "stapik/settings/SettingsStore.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace stapik::ui
{
    [[nodiscard]] std::optional<std::filesystem::path> loadLastDirectory(const settings::SettingsStore& store, const std::string& key);
    [[nodiscard]] storage::FileResult saveLastDirectory(settings::SettingsStore& store, const std::string& key, const std::filesystem::path& chosen);
}
