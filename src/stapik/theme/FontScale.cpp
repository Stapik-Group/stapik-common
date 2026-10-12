#include "FontScale.hpp"

#include <algorithm>
#include <format>
#include <regex>

namespace
{
    constexpr double PERCENT_DIVISOR = 100.0;

    const std::regex& pixelFontSize()
    {
        static const std::regex pattern(R"((font-size\s*:\s*)([0-9]+(?:\.[0-9]+)?)px)");
        return pattern;
    }
}

namespace stapik::theme
{
    int clampFontScalePercent(const int percent)
    {
        return std::clamp(percent, MIN_FONT_SCALE_PERCENT, MAX_FONT_SCALE_PERCENT);
    }

    std::string scaleFontSizes(const std::string_view css, const int percent)
    {
        const auto factor = clampFontScalePercent(percent) / PERCENT_DIVISOR;
        const std::string source(css);

        std::string result;
        auto last = source.cbegin();
        for (std::sregex_iterator match(source.cbegin(), source.cend(), pixelFontSize()), end; match != end; ++match)
        {
            const auto size = std::stod((*match)[2].str()) * factor;
            result.append(last, (*match)[0].first);
            result += std::format("{}{:.1f}px", (*match)[1].str(), size);
            last = (*match)[0].second;
        }

        result.append(last, source.cend());
        return result;
    }
}
