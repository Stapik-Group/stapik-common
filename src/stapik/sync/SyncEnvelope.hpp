#pragma once

#include <chrono>
#include <nlohmann/json.hpp>

namespace stapik::sync
{
    struct SyncEnvelope
    {
        std::chrono::system_clock::time_point lastUpdate;
        nlohmann::json payload;

        [[nodiscard]] nlohmann::json toJson() const;
        [[nodiscard]] static SyncEnvelope fromJson(const nlohmann::json& json);
    };
}