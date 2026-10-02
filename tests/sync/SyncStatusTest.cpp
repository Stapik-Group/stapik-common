#include "stapik/sync/SyncStatus.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    using stapik::sync::SyncState;
    using stapik::sync::SyncStatus;
    using stapik::sync::statusFor;
    using stapik::sync::syncStatusKey;
}

TEST(SyncStatusTest, EveryStatusHasADistinctLocaleKey)
{
    const SyncStatus all[] = { SyncStatus::Idle, SyncStatus::Syncing, SyncStatus::Offline, SyncStatus::Conflict, SyncStatus::Error };

    for (std::size_t first = 0; first < std::size(all); ++first)
    {
        EXPECT_EQ(std::string(syncStatusKey(all[first])).rfind("sync.status.", 0), 0u);

        for (std::size_t second = first + 1; second < std::size(all); ++second)
            EXPECT_NE(std::string(syncStatusKey(all[first])), std::string(syncStatusKey(all[second])));
    }
}

TEST(SyncStatusTest, OutcomesMapToStatuses)
{
    EXPECT_EQ(statusFor(SyncState::Synchronized), SyncStatus::Idle);
    EXPECT_EQ(statusFor(SyncState::ServerWon), SyncStatus::Idle);
    EXPECT_EQ(statusFor(SyncState::OfflineKeptLocal), SyncStatus::Offline);
    EXPECT_EQ(statusFor(SyncState::LostRaceAcceptedServer), SyncStatus::Conflict);
    EXPECT_EQ(statusFor(SyncState::RemoteUnreadable), SyncStatus::Error);
}
