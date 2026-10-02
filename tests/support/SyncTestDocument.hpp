#pragma once

#include "stapik/sync/SyncCoordinator.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace stapik::test
{
    using TimePoint = std::chrono::system_clock::time_point;

    inline TimePoint at(const int seconds)
    {
        using namespace std::chrono;
        return sys_days{ year{ 2026 } / January / 1 } + hours{ 12 } + std::chrono::seconds{ seconds };
    }

    struct Entry
    {
        std::string text;
        TimePoint updated;
        std::optional<TimePoint> cloudBaseline;

        [[nodiscard]] nlohmann::json toJson() const
        {
            nlohmann::json json = nlohmann::json::object();
            json["text"] = text;
            return json;
        }

        static Entry fromJson(const nlohmann::json& json)
        {
            return Entry{ json.at("text").get<std::string>(), TimePoint{}, std::nullopt };
        }

        [[nodiscard]] TimePoint lastUpdate() const { return updated; }
        [[nodiscard]] std::optional<TimePoint> lastKnownCloudUpdate() const { return cloudBaseline; }

        [[nodiscard]] Entry withLastKnownCloudUpdate(const TimePoint cloudUpdate) const
        {
            return Entry{ text, updated, cloudUpdate };
        }
    };

    static_assert(stapik::sync::SyncableDocument<Entry>);

    inline Entry local(const std::string& text, const int updatedAt, const std::optional<int> baseline = std::nullopt)
    {
        return Entry{ text, at(updatedAt), baseline ? std::optional<TimePoint>{ at(*baseline) } : std::nullopt };
    }

    inline nlohmann::json cloudContent(const std::string& text, const TimePoint contentTime)
    {
        return stapik::sync::SyncEnvelope{ contentTime, Entry{ text, contentTime, std::nullopt }.toJson() }.toJson();
    }

    inline CloudDocument cloudDocument(const std::string& text, const int contentTime, const int serverTime)
    {
        return CloudDocument{ .content = cloudContent(text, at(contentTime)), .updatedAt = at(serverTime) };
    }
}
