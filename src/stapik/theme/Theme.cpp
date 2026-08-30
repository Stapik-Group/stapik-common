#include "Theme.hpp"

std::string themeToFileString(const Theme theme)
{
    return theme == Theme::Modern ? "modern" : "classic";
}

Theme themeFromFileString(const std::string& value)
{
    return value == "modern" ? Theme::Modern : Theme::Classic;
}