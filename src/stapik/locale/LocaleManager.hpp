#pragma once

#include "stapik/locale/Locale.hpp"
#include "stapik/locale/LocalizationEngine.hpp"
#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>
#include <string>
#include <string_view>
#include <vector>

class LocaleManager
{
public:
    static LocaleManager& instance();
    static LocaleManager& instance(const std::string& appName);
    void setLocale(Locale locale);
    [[nodiscard]] Locale getLocale() const;
    void setLanguage(const std::string& languageCode);
    [[nodiscard]] const std::string& languageCode() const;
    [[nodiscard]] std::string translate(std::string_view key) const;
    [[nodiscard]] std::string translate(std::string_view key, const LocalizationEngine::Arguments& arguments) const;
    [[nodiscard]] const std::vector<stapik::locale::LanguageInfo>& languages() const;
    sigc::signal<void()>& signalLocaleChanged();
private:
    explicit LocaleManager(const std::string& appName);
    LocalizationEngine m_engine;
    stapik::settings::ObservableSetting<std::string> m_languageCode;
    sigc::signal<void()> m_signalLocaleChanged;
};
