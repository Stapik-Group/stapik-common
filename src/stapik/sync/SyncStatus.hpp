#pragma once

#include "SyncOutcome.hpp"

#include <array>

namespace stapik::sync
{
    enum class SyncStatus
    {
        Idle,
        Syncing,
        Offline,
        Conflict,
        Error
    };

    inline constexpr std::array<SyncStatus, 5> ALL_SYNC_STATUSES = {
        SyncStatus::Idle, SyncStatus::Syncing, SyncStatus::Offline, SyncStatus::Conflict, SyncStatus::Error
    };

    [[nodiscard]] constexpr const char* syncStatusCssClass(const SyncStatus status)
    {
        switch (status)
        {
            using enum SyncStatus;
            case Idle: return "stapik-sync-idle";
            case Syncing: return "stapik-sync-syncing";
            case Offline: return "stapik-sync-offline";
            case Conflict: return "stapik-sync-conflict";
            case Error: return "stapik-sync-error";
        }

        return "stapik-sync-error";
    }

    [[nodiscard]] constexpr const char* syncStatusKey(const SyncStatus status)
    {
        switch (status)
        {
            case SyncStatus::Idle: return "sync.status.idle";
            case SyncStatus::Syncing: return "sync.status.syncing";
            case SyncStatus::Offline: return "sync.status.offline";
            case SyncStatus::Conflict: return "sync.status.conflict";
            case SyncStatus::Error: return "sync.status.error";
        }

        return "sync.status.error";
    }

    [[nodiscard]] constexpr SyncStatus statusFor(const SyncState state)
    {
        switch (state)
        {
            case SyncState::Synchronized: return SyncStatus::Idle;
            case SyncState::ServerWon: return SyncStatus::Idle;
            case SyncState::OfflineKeptLocal: return SyncStatus::Offline;
            case SyncState::LostRaceAcceptedServer: return SyncStatus::Conflict;
            case SyncState::RemoteUnreadable: return SyncStatus::Error;
        }

        return SyncStatus::Error;
    }
}
