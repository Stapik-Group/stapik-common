#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace stapik::sync
{
    enum class SyncState
    {
        Synchronized,
        OfflineKeptLocal,
        ServerWon,
        LostRaceAcceptedServer,
        RemoteUnreadable
    };

    template<typename DocumentType>
    struct SyncOutcome
    {
        SyncState state;
        DocumentType document;
        std::string message;
        // Set when the cloud asked to wait before the next request (rate limit); the next attempt should not come sooner.
        std::optional<std::chrono::seconds> retryAfter;

        SyncOutcome(const SyncState outcomeState, DocumentType outcomeDocument, std::string outcomeMessage = {}, const std::optional<std::chrono::seconds> outcomeRetryAfter = std::nullopt) :
            state(outcomeState),
            document(std::move(outcomeDocument)),
            message(std::move(outcomeMessage)),
            retryAfter(outcomeRetryAfter)
        {}

        [[nodiscard]] bool replacesLocal() const
        {
            return state == SyncState::ServerWon || state == SyncState::LostRaceAcceptedServer;
        }
    };
}
