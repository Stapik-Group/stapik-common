#include "ThemeManager.hpp"
#include "stapik/storage/AppPaths.hpp"
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
    const auto path = themeConfigPath();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << toFileString(theme);
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
    return fromFileString(content);
}

std::filesystem::path ThemeManager::themeConfigPath() const
{
    return AppPaths::userDataDir(m_appName) / "theme.txt";
}