#pragma once

#include "stapik/locale/Locale.hpp"
#include "stapik/locale/LocalizationEngine.hpp"

#include <sigc++/signal.h>
#include <filesystem>
#include <string>

class LocaleManager
{
public:
    static LocaleManager& instance(const std::string& appName = "");
    void setLocale(Locale locale);
    [[nodiscard]] Locale getLocale() const;
    [[nodiscard]] std::string translate(const std::string& key) const;
    sigc::signal<void()>& signalLocaleChanged();
private:
    explicit LocaleManager(std::string  appName);
    LocalizationEngine m_engine;
    std::string m_appName;
    sigc::signal<void()> m_signalLocaleChanged;
    void saveLocale(Locale locale) const;
    [[nodiscard]] Locale loadSavedLocale() const;
    [[nodiscard]] std::filesystem::path localeConfigPath() const;
};