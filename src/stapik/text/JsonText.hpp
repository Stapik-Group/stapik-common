#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace stapik::text
{
    [[nodiscard]] inline std::string dumpJson(const nlohmann::json& json, const int indent = -1)
    {
        return json.dump(indent, ' ', false, nlohmann::json::error_handler_t::replace);
    }
}
