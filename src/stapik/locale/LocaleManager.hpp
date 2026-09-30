#pragma once

#include "stapik/locale/Locale.hpp"
#include "stapik/locale/LocalizationEngine.hpp"
#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>
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
    explicit LocaleManager(const std::string& appName);
    LocalizationEngine m_engine;
    stapik::settings::ObservableSetting<Locale> m_locale;
    sigc::signal<void()> m_signalLocaleChanged;
};
