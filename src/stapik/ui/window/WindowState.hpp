#pragma once

#include "stapik/settings/SettingsStore.hpp"

#include <optional>
#include <string>

namespace stapik::ui
{
    struct WindowPosition
    {
        int x = 0;
        int y = 0;

        bool operator==(const WindowPosition&) const = default;
    };

    struct WindowState
    {
        static constexpr int MIN_DIMENSION = 100;
        static constexpr int MAX_DIMENSION = 32768;
        static constexpr int MAX_COORDINATE = 100000;

        int width = 0;
        int height = 0;
        bool maximized = false;
        std::optional<WindowPosition> position;

        [[nodiscard]] bool hasSize() const;
        [[nodiscard]] bool hasPosition() const;
        [[nodiscard]] static std::optional<WindowState> load(const settings::SettingsStore& store, const std::string& key);

        bool save(settings::SettingsStore& store, const std::string& key) const;
    };
}
