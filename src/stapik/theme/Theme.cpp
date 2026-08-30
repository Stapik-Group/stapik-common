#include "Theme.hpp"

std::string themeToFileString(const Theme theme)
{
    switch (theme)
    {
        using enum Theme;
        case Modern: return "modern";
        case ClassicPink: return "classic-pink";
        case Classic:
        default: return "classic";
    }
}

Theme themeFromFileString(const std::string& value)
{
    using enum Theme;
    if (value == "modern") return Modern;
    if (value == "classic-pink") return ClassicPink;
    return Classic;
}