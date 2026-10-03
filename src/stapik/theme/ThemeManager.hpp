#pragma once

#include "Theme.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>
#include <string>

class ThemeManager
{
public:
    static ThemeManager& instance();
    static ThemeManager& instance(const std::string& appName);
    void setTheme(Theme theme);
    void setThemeId(const std::string& themeId);
    sigc::signal<void()>& signalThemeChanged();
    [[nodiscard]] Theme getTheme() const;
    [[nodiscard]] const std::string& themeId() const;
private:
    explicit ThemeManager(const std::string& appName);
    stapik::settings::ObservableSetting<std::string> m_themeId;
    sigc::signal<void()> m_signalThemeChanged;
};
