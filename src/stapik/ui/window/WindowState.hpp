#pragma once

#include "stapik/settings/SettingsStore.hpp"

#include <optional>
#include <string>

namespace stapik::ui
{
    struct WindowState
    {
        static constexpr int MIN_DIMENSION = 100;
        static constexpr int MAX_DIMENSION = 32768;

        int width = 0;
        int height = 0;
        bool maximized = false;

        [[nodiscard]] bool hasSize() const;
        [[nodiscard]] static std::optional<WindowState> load(const settings::SettingsStore& store, const std::string& key);

        bool save(settings::SettingsStore& store, const std::string& key) const;
    };
}
