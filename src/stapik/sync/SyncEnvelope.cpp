#include "SyncEnvelope.hpp"
#include "Timestamp.hpp"

namespace stapik::sync
{
    nlohmann::json SyncEnvelope::toJson() const
    {
        return nlohmann::json{
                { "lastUpdate", toIso8601(lastUpdate) },
                { "payload", payload }
        };
    }

    SyncEnvelope SyncEnvelope::fromJson(const nlohmann::json& json)
    {
        SyncEnvelope envelope;
        envelope.lastUpdate = json.contains("lastUpdate")
            ? fromIso8601(json.at("lastUpdate").get<std::string>())
            : std::chrono::system_clock::time_point{};
        envelope.payload = json.contains("payload") ? json.at("payload") : nlohmann::json::array();
        return envelope;
    }
}