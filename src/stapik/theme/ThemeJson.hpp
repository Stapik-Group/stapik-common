#pragma once

#include "Theme.hpp"

#include <nlohmann/json.hpp>

#include <string>

template<>
struct nlohmann::adl_serializer<Theme>
{
    static void to_json(json& target, const Theme& theme)
    {
        target = themeToFileString(theme);
    }

    static void from_json(const json& source, Theme& theme)
    {
        theme = themeFromFileString(source.get<std::string>());
    }
};
