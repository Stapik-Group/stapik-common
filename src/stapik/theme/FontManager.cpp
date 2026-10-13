#include "FontManager.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/settings/AppSettings.hpp"

FontManager& FontManager::instance()
{
    static FontManager manager;
    return manager;
}

FontManager::FontManager() :
    m_font(
        stapik::settings::appSettingsStore(stapik::app::AppContext::instance().info().internalName),
        "font",
        std::string())
{
    m_font.signalChanged().connect([this](const std::string&) { m_signalFontChanged.emit(); });
}

void FontManager::setFont(const std::string& fontText)
{
    m_font.set(fontText);
}

const std::string& FontManager::fontText() const
{
    return m_font.get();
}

stapik::settings::ObservableSetting<std::string>& FontManager::fontSetting()
{
    return m_font;
}

sigc::signal<void()>& FontManager::signalFontChanged()
{
    return m_signalFontChanged;
}
