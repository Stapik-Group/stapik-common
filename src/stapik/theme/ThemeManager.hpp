#pragma once
#include "Theme.hpp"
#include <sigc++/signal.h>
#include <filesystem>
#include <string>

class ThemeManager
{
public:
    static ThemeManager& instance(const std::string& appName = "");
    void setTheme(Theme theme);
    [[nodiscard]] Theme getTheme() const;
    sigc::signal<void()>& signalThemeChanged();
private:
    explicit ThemeManager(std::string appName);
    Theme m_theme;
    std::string m_appName;
    sigc::signal<void()> m_signalThemeChanged;
    void saveTheme(Theme theme) const;
    [[nodiscard]] Theme loadSavedTheme() const;
    [[nodiscard]] std::filesystem::path themeConfigPath() const;
};