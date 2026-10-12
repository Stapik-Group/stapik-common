#pragma once

#include <string>
#include <string_view>

namespace stapik::theme
{
    constexpr int MIN_FONT_SCALE_PERCENT = 70;
    constexpr int DEFAULT_FONT_SCALE_PERCENT = 100;
    constexpr int MAX_FONT_SCALE_PERCENT = 200;

    [[nodiscard]] int clampFontScalePercent(int percent);
    [[nodiscard]] std::string scaleFontSizes(std::string_view css, int percent);
}
