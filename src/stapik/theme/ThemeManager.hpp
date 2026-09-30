#pragma once

#include "Theme.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>
#include <string>

class ThemeManager
{
public:
    static ThemeManager& instance(const std::string& appName = "");
    void setTheme(Theme theme);
    [[nodiscard]] Theme getTheme() const;
    sigc::signal<void()>& signalThemeChanged();
private:
    explicit ThemeManager(const std::string& appName);
    stapik::settings::ObservableSetting<Theme> m_theme;
    sigc::signal<void()> m_signalThemeChanged;
};
