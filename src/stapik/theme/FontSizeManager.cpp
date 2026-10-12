#include "FontSizeManager.hpp"

#include "FontScale.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/settings/AppSettings.hpp"

FontSizeManager& FontSizeManager::instance()
{
    static FontSizeManager manager;
    return manager;
}

FontSizeManager::FontSizeManager() :
    m_scalePercent(
        stapik::settings::appSettingsStore(stapik::app::AppContext::instance().info().internalName),
        "fontScale",
        stapik::theme::DEFAULT_FONT_SCALE_PERCENT)
{
    m_scalePercent.signalChanged().connect([this](const int) { m_signalFontSizeChanged.emit(); });
}

void FontSizeManager::setScalePercent(const int percent)
{
    m_scalePercent.set(stapik::theme::clampFontScalePercent(percent));
}

int FontSizeManager::scalePercent() const
{
    return stapik::theme::clampFontScalePercent(m_scalePercent.get());
}

stapik::settings::ObservableSetting<int>& FontSizeManager::scaleSetting()
{
    return m_scalePercent;
}

sigc::signal<void()>& FontSizeManager::signalFontSizeChanged()
{
    return m_signalFontSizeChanged;
}
