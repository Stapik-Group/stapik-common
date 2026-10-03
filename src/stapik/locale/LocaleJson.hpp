#pragma once

#include "Locale.hpp"

#include <nlohmann/json.hpp>

#include <string>

template<>
struct nlohmann::adl_serializer<Locale>
{
    static void to_json(json& target, const Locale& locale)
    {
        target = std::string(toFileString(locale));
    }

    static void from_json(const json& source, Locale& locale)
    {
        locale = fromFileString(source.get<std::string>());
    }
};
