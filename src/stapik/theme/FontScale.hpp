#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace stapik::theme
{
    constexpr double MIN_FONT_SCALE_FACTOR = 0.25;
    constexpr double MAX_FONT_SCALE_FACTOR = 4.0;

    // Multiplies every `font-size: <n>px` of the CSS by the factor (limited to the range above).
    [[nodiscard]] std::string scaleFontSizes(std::string_view css, double factor);
    // The first `font-size: <n>px` of a theme file. In every theme it is the one of the `*` rule: the base size.
    [[nodiscard]] std::optional<double> baseFontSizePixels(std::string_view css);
}
