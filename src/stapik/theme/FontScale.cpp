#include "FontScale.hpp"

#include <algorithm>
#include <format>
#include <regex>

namespace
{
    const std::regex& pixelFontSize()
    {
        static const std::regex pattern(R"((font-size\s*:\s*)([0-9]+(?:\.[0-9]+)?)px)");
        return pattern;
    }
}

namespace stapik::theme
{
    std::string scaleFontSizes(const std::string_view css, const double factor)
    {
        const auto limitedFactor = std::clamp(factor, MIN_FONT_SCALE_FACTOR, MAX_FONT_SCALE_FACTOR);
        const std::string source(css);

        std::string result;
        auto last = source.cbegin();
        for (std::sregex_iterator match(source.cbegin(), source.cend(), pixelFontSize()), end; match != end; ++match)
        {
            const auto size = std::stod((*match)[2].str()) * limitedFactor;
            result.append(last, (*match)[0].first);
            result += std::format("{}{:.1f}px", (*match)[1].str(), size);
            last = (*match)[0].second;
        }

        result.append(last, source.cend());
        return result;
    }

    std::optional<double> baseFontSizePixels(const std::string_view css)
    {
        const std::string source(css);

        std::smatch match;
        if (!std::regex_search(source, match, pixelFontSize()))
            return std::nullopt;

        return std::stod(match[2].str());
    }
}
