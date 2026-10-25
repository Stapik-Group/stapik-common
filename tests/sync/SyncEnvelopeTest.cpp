#include "stapik/sync/SyncEnvelope.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <tuple>

namespace
{
    using namespace std::chrono;
    using stapik::sync::SyncEnvelope;

    const system_clock::time_point UPDATED = sys_days{ year{ 2026 } / January / 15 } + 10h + 30min;
}

TEST(SyncEnvelopeTest, RoundTripsTheLastUpdateAndThePayload)
{
    const SyncEnvelope original{ UPDATED, nlohmann::json{ { "text", "hello" } } };

    const auto restored = SyncEnvelope::fromJson(original.toJson());

    EXPECT_EQ(restored.lastUpdate, UPDATED);
    EXPECT_EQ(restored.payload, original.payload);
}

TEST(SyncEnvelopeTest, RejectsAnInvalidLastUpdate)
{
    for (const auto* text : { "yesterday", "", "2026-13-01T00:00:00Z", "2026-01-15" })
    {
        const nlohmann::json json{ { "lastUpdate", text }, { "payload", nlohmann::json::object() } };

        EXPECT_THROW(std::ignore = SyncEnvelope::fromJson(json), std::exception) << text;
    }
}

TEST(SyncEnvelopeTest, RejectsAMissingLastUpdate)
{
    const nlohmann::json json{ { "payload", nlohmann::json::object() } };

    EXPECT_THROW(std::ignore = SyncEnvelope::fromJson(json), std::exception);
}

TEST(SyncEnvelopeTest, RejectsALastUpdateThatIsNotText)
{
    const nlohmann::json json{ { "lastUpdate", 1700000000 }, { "payload", nlohmann::json::object() } };

    EXPECT_THROW(std::ignore = SyncEnvelope::fromJson(json), std::exception);
}

TEST(SyncEnvelopeTest, MissingPayloadReadsAsAnEmptyList)
{
    const nlohmann::json json{ { "lastUpdate", "2026-01-15T10:30:00Z" } };

    EXPECT_EQ(SyncEnvelope::fromJson(json).payload, nlohmann::json::array());
}
