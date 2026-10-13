#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace stapik::theme
{
    inline constexpr double MIN_FONT_SIZE_POINTS = 6.0;
    inline constexpr double MAX_FONT_SIZE_POINTS = 40.0;
    // The pixels of a CSS file are the logical pixels of GTK, at 96 per inch; a point is 1/72 inch.
    inline constexpr double CSS_PIXELS_PER_POINT = 96.0 / 72.0;

    // A font picked by the user: a family and a size. The family is one name, not a list.
    struct FontChoice
    {
        std::string family;
        double sizePoints = 0.0;

        bool operator==(const FontChoice&) const = default;
    };

    // The text kept in the settings: the family and the size, for example "Inter 11" or "Noto Sans 10.5".
    [[nodiscard]] std::string formatFontChoice(const FontChoice& choice);
    // Nothing for an empty text, which means the font of the theme, and for a text that is not a font.
    // A size outside of the supported range is moved into it.
    [[nodiscard]] std::optional<FontChoice> parseFontChoice(std::string_view text);
    // The family as a CSS value: quoted, with a generic family behind it for the characters it lacks.
    [[nodiscard]] std::string fontFamilyCss(std::string_view family);
}
