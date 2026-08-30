#include "Theme.hpp"

std::string toFileString(const Theme theme)
{
    return theme == Theme::Modern ? "modern" : "classic";
}

Theme fromFileString(const std::string& value)
{
    return value == "modern" ? Theme::Modern : Theme::Classic;
}