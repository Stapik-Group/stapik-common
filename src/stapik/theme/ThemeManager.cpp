#include "ThemeManager.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/log/Log.hpp"

#include <fstream>
#include <utility>

ThemeManager& ThemeManager::instance(const std::string& appName)
{
    static ThemeManager manager(appName);
    return manager;
}

ThemeManager::ThemeManager(std::string appName) :
    m_appName(std::move(appName))
{
    m_theme = loadSavedTheme();
}

void ThemeManager::setTheme(const Theme theme)
{
    m_theme = theme;
    saveTheme(theme);
    m_signalThemeChanged.emit();
}

Theme ThemeManager::getTheme() const
{
    return m_theme;
}

sigc::signal<void()>& ThemeManager::signalThemeChanged()
{
    return m_signalThemeChanged;
}

void ThemeManager::saveTheme(const Theme theme) const
{
    if (!stapik::storage::writeFileAtomically(themeConfigPath(), themeToFileString(theme)))
        stapik::log::warning("Cannot save theme setting");
}

Theme ThemeManager::loadSavedTheme() const
{
    const auto path = themeConfigPath();
    if (!std::filesystem::exists(path))
        return Theme::Classic;
    std::ifstream file(path);
    if (!file.is_open())
        return Theme::Classic;
    std::string content;
    file >> content;
    return themeFromFileString(content);
}

std::filesystem::path ThemeManager::themeConfigPath() const
{
    return AppPaths::userDataDir(m_appName) / "theme.txt";
}