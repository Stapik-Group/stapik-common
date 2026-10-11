#pragma once

#include "WindowState.hpp"

#include "stapik/settings/SettingsStore.hpp"

#include <gtkmm/paned.h>

#include <optional>
#include <string>

namespace stapik::ui
{
    inline constexpr int MAX_PANED_POSITION = WindowState::MAX_DIMENSION;
    [[nodiscard]] std::optional<int> loadPanedPosition(const settings::SettingsStore& store, const std::string& key);
    bool savePanedPosition(settings::SettingsStore& store, const std::string& key, int position);
    void rememberPanedPosition(Gtk::Paned& paned, settings::SettingsStore& store, const std::string& key);
}
