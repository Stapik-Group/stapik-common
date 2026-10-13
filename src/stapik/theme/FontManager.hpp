#pragma once

#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>

#include <string>

// The font chosen for the whole application. The setting holds the text of a FontChoice ("Inter 11"),
// or nothing, which keeps the font and the sizes of the theme.
class FontManager
{
public:
    static FontManager& instance();
    void setFont(const std::string& fontText);
    [[nodiscard]] const std::string& fontText() const;
    [[nodiscard]] stapik::settings::ObservableSetting<std::string>& fontSetting();
    sigc::signal<void()>& signalFontChanged();
private:
    FontManager();
    stapik::settings::ObservableSetting<std::string> m_font;
    sigc::signal<void()> m_signalFontChanged;
};
