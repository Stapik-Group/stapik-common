#include "LocaleManager.hpp"

#include "stapik/storage/AppPaths.hpp"

#include <fstream>
#include <utility>

LocaleManager& LocaleManager::instance(const std::string& appName)
{
    static LocaleManager manager(appName);
    return manager;
}

LocaleManager::LocaleManager(std::string appName) :
    m_engine(AppPaths::resourcesDir() / "locales"),
    m_appName(std::move(appName))
{
    m_engine.setLocale(loadSavedLocale());
}

void LocaleManager::setLocale(const Locale locale)
{
    m_engine.setLocale(locale);
    saveLocale(locale);
    m_signalLocaleChanged.emit();
}

Locale LocaleManager::getLocale() const
{
    return m_engine.getLocale();
}

std::string LocaleManager::translate(const std::string &key) const
{
    return m_engine.translate(key);
}

sigc::signal<void()>& LocaleManager::signalLocaleChanged()
{
    return m_signalLocaleChanged;
}

void LocaleManager::saveLocale(const Locale locale) const
{
    const auto path = localeConfigPath();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << toFileString(locale);
}

Locale LocaleManager::loadSavedLocale() const
{
    const auto path = localeConfigPath();
    if (!std::filesystem::exists(path))
        return Locale::EN;

    std::ifstream file(path);
    if (!file.is_open())
        return Locale::EN;

    std::string content;
    file >> content;

    return fromFileString(content);
}

std::filesystem::path LocaleManager::localeConfigPath() const
{
    return AppPaths::userDataDir(m_appName) / "locale.txt";
}