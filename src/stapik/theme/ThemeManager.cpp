#include "ThemeManager.hpp"

#include "stapik/settings/AppSettings.hpp"
#include "stapik/storage/AppPaths.hpp"

namespace
{
    stapik::settings::SettingsStore& themeSettingsStore(const std::string& appName)
    {
        auto& store = stapik::settings::appSettingsStore(appName);

        stapik::settings::importLegacyTextSetting(
            store,
            "theme",
            AppPaths::userDataDir(appName) / "theme.txt",
            [](const std::string& value) { return themeToFileString(themeFromFileString(value)); });

        return store;
    }
}

ThemeManager& ThemeManager::instance(const std::string& appName)
{
    static ThemeManager manager(appName);
    return manager;
}

ThemeManager::ThemeManager(const std::string& appName) :
    m_theme(themeSettingsStore(appName), "theme", Theme::Classic)
{}

void ThemeManager::setTheme(const Theme theme)
{
    m_theme.set(theme);
    m_signalThemeChanged.emit();
}

Theme ThemeManager::getTheme() const
{
    return m_theme.get();
}

sigc::signal<void()>& ThemeManager::signalThemeChanged()
{
    return m_signalThemeChanged;
}
