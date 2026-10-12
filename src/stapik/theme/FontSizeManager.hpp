#pragma once

#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/signal.h>

class FontSizeManager
{
public:
    static FontSizeManager& instance();
    void setScalePercent(int percent);
    [[nodiscard]] int scalePercent() const;
    [[nodiscard]] stapik::settings::ObservableSetting<int>& scaleSetting();
    sigc::signal<void()>& signalFontSizeChanged();
private:
    FontSizeManager();
    stapik::settings::ObservableSetting<int> m_scalePercent;
    sigc::signal<void()> m_signalFontSizeChanged;
};
