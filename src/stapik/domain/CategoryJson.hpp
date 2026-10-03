#pragma once

#include "Category.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

// Format: { "id": "...", "name": "...", "color": "red" }. An unknown color id is a format error.
template<>
struct nlohmann::adl_serializer<stapik::domain::Category>
{
    static void to_json(json& target, const stapik::domain::Category& category)
    {
        target = json{
            {"id", category.id},
            {"name", category.name},
            {"color", std::string(stapik::domain::categoryColorId(category.color))}};
    }

    static void from_json(const json& source, stapik::domain::Category& category)
    {
        const auto colorId = source.at("color").get<std::string>();
        const auto color = stapik::domain::categoryColorFromId(colorId);
        if (!color)
            throw std::invalid_argument("Unknown category color: " + colorId);

        category.id = source.at("id").get<std::string>();
        category.name = source.at("name").get<std::string>();
        category.color = *color;
    }
};
