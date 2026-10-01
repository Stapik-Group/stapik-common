#pragma once

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

        SyncOutcome(const SyncState outcomeState, DocumentType outcomeDocument, std::string outcomeMessage = {}) :
            state(outcomeState),
            document(std::move(outcomeDocument)),
            message(std::move(outcomeMessage))
        {}

        [[nodiscard]] bool replacesLocal() const
        {
            return state == SyncState::ServerWon || state == SyncState::LostRaceAcceptedServer;
        }
    };
}
