#include "FontChoice.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <system_error>

namespace
{
    constexpr std::string_view WHITESPACE = " \t";

    std::string_view trimmed(std::string_view text)
    {
        const auto begin = text.find_first_not_of(WHITESPACE);
        if (begin == std::string_view::npos)
            return {};

        return text.substr(begin, text.find_last_not_of(WHITESPACE) - begin + 1);
    }

    bool hasControlCharacter(const std::string_view text)
    {
        return std::ranges::any_of(text, [](const char character)
        {
            return static_cast<unsigned char>(character) < 0x20;
        });
    }
}

namespace stapik::theme
{
    std::string formatFontChoice(const FontChoice& choice)
    {
        return std::format("{} {:g}", choice.family, choice.sizePoints);
    }

    std::optional<FontChoice> parseFontChoice(const std::string_view text)
    {
        const auto source = trimmed(text);
        const auto separator = source.find_last_of(WHITESPACE);
        if (separator == std::string_view::npos)
            return std::nullopt;

        const auto family = trimmed(source.substr(0, separator));
        const auto sizeText = source.substr(separator + 1);
        if (family.empty() || hasControlCharacter(family))
            return std::nullopt;

        double size = 0.0;
        const auto [end, error] = std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size);
        if (error != std::errc{} || end != sizeText.data() + sizeText.size() || !std::isfinite(size))
            return std::nullopt;

        return FontChoice{
            .family = std::string(family),
            .sizePoints = std::clamp(size, MIN_FONT_SIZE_POINTS, MAX_FONT_SIZE_POINTS)
        };
    }

    std::string fontFamilyCss(const std::string_view family)
    {
        std::string quoted = "\"";
        for (const char character : family)
        {
            if (character == '"' || character == '\\')
                quoted += '\\';

            quoted += character;
        }

        quoted += "\", sans-serif";
        return quoted;
    }
}
