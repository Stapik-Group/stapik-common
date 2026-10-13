#include "CloudChangesProtocol.hpp"
#include "CloudPartitionProtocol.hpp"
#include "CloudStorageException.hpp"

#include "stapik/sync/Timestamp.hpp"

#include <nlohmann/json.hpp>

namespace stapik::cloud
{
    namespace
    {
        std::chrono::system_clock::time_point parseTime(const nlohmann::json& json, const char* field)
        {
            const auto text = json.at(field).get<std::string>();
            const auto time = sync::parseIso8601(text);
            if (!time)
                throw CloudStorageException("Invalid ISO-8601 timestamp: " + text);

            return *time;
        }
    }

    std::string changesUrl(const std::string& apiUrl, const std::chrono::system_clock::time_point since)
    {
        // Microseconds, because serverTime of one answer is passed back as `since` of the next one and must not lose precision.
        return apiUrl + "/api/v1/changes?since=" + encodePathSegment(sync::toIso8601(since, sync::TimestampPrecision::Microseconds));
    }

    CloudChangeList parseChangeList(const std::string& body)
    {
        try
        {
            const auto json = nlohmann::json::parse(body);

            CloudChangeList result{ .serverTime = parseTime(json, "serverTime") };
            for (const auto& item : json.at("changes"))
            {
                CloudChange change{
                    .slotKey = item.at("slotKey").get<std::string>(),
                    .sizeBytes = item.at("sizeBytes").get<std::int64_t>(),
                    .contentHash = item.at("contentHash").get<std::string>(),
                    .updatedAt = parseTime(item, "updatedAt"),
                    .deleted = item.at("deleted").get<bool>()
                };

                if (item.contains("partition") && !item.at("partition").is_null())
                    change.partition = item.at("partition").get<std::string>();

                result.changes.push_back(std::move(change));
            }

            return result;
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }
}
