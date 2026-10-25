#include "SyncEnvelope.hpp"
#include "Timestamp.hpp"

#include <stdexcept>
#include <string>

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
        const auto lastUpdateText = json.at("lastUpdate").get<std::string>();
        const auto lastUpdate = parseIso8601(lastUpdateText);
        if (!lastUpdate)
            throw std::invalid_argument("The envelope has an invalid lastUpdate: \"" + lastUpdateText + "\"");

        SyncEnvelope envelope;
        envelope.lastUpdate = *lastUpdate;
        envelope.payload = json.contains("payload") ? json.at("payload") : nlohmann::json::array();
        return envelope;
    }
}