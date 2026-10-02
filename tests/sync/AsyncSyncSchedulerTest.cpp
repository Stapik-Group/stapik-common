#include "stapik/sync/AsyncSyncScheduler.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/MainLoopPump.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using stapik::sync::AsyncSyncOptions;
    using stapik::sync::AsyncSyncScheduler;
    using stapik::sync::SyncEnvelope;
    using stapik::sync::SyncState;
    using stapik::sync::SyncStatus;
    using stapik::sync::backoffDelay;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;
    using stapik::test::TimePoint;

    class AsyncSyncSchedulerTest : public testing::Test
    {
    protected:
        AsyncSyncSchedulerTest()
        {
            m_fake.nextServerTime = at(1000);
            m_fake.serverTimeStep = 1s;
        }

        void watch(AsyncSyncScheduler<Entry>& scheduler)
        {
            scheduler.signalStatusChanged().connect([this](const SyncStatus status) { m_statuses.push_back(status); });
            scheduler.signalOutcome().connect([this](const stapik::sync::SyncOutcome<Entry>& outcome)
            {
                m_outcomes.push_back({ outcome.state, outcome.document });
            });
        }

        [[nodiscard]] std::string storedText() const
        {
            return SyncEnvelope::fromJson(m_fake.stored->content).payload.at("text").get<std::string>();
        }

        [[nodiscard]] bool sawStatus(const SyncStatus status) const
        {
            return std::ranges::find(m_statuses, status) != m_statuses.end();
        }

        struct Recorded
        {
            SyncState state;
            Entry document;
        };

        AsyncSyncOptions m_options{ 30ms, 40ms, 160ms };
        FakeCloudStorage m_fake;
        std::vector<SyncStatus> m_statuses;
        std::vector<Recorded> m_outcomes;
    };
}

TEST_F(AsyncSyncSchedulerTest, ChangesMadeInQuickSuccessionAreUploadedOnce)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    for (int index = 0; index < 5; ++index)
        scheduler.documentChanged(local("v" + std::to_string(index), 100 + index));

    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));
    pumpFor(150ms);

    EXPECT_EQ(m_fake.saveCalls, 1);
    EXPECT_EQ(storedText(), "v4");
}

TEST_F(AsyncSyncSchedulerTest, StatusGoesFromSyncingToIdle)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.documentChanged(local("a", 100));
    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));

    EXPECT_EQ(m_statuses, (std::vector<SyncStatus>{ SyncStatus::Syncing, SyncStatus::Idle }));
    EXPECT_EQ(scheduler.status(), SyncStatus::Idle);
    EXPECT_FALSE(scheduler.hasPendingWork());
}

TEST_F(AsyncSyncSchedulerTest, OutcomeCarriesTheServerBaseline)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.documentChanged(local("a", 100));
    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));

    EXPECT_EQ(m_outcomes.front().state, SyncState::Synchronized);
    EXPECT_EQ(m_outcomes.front().document.cloudBaseline, std::optional<TimePoint>{ at(1000) });
}

TEST_F(AsyncSyncSchedulerTest, OfflineUploadIsRetriedUntilItSucceeds)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);
    m_fake.unreachable = true;

    scheduler.documentChanged(local("offline edit", 100));
    ASSERT_TRUE(pumpUntil([this] { return sawStatus(SyncStatus::Offline); }));
    EXPECT_TRUE(scheduler.hasPendingWork());

    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([this] { return m_outcomes.back().state == SyncState::Synchronized; }));

    EXPECT_EQ(scheduler.status(), SyncStatus::Idle);
    EXPECT_GE(m_fake.saveCalls, 2);
    EXPECT_EQ(storedText(), "offline edit");
}

TEST_F(AsyncSyncSchedulerTest, ChangeDuringOfflineRetryUploadsTheLatestDocument)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);
    m_fake.unreachable = true;

    scheduler.documentChanged(local("first", 100));
    ASSERT_TRUE(pumpUntil([this] { return sawStatus(SyncStatus::Offline); }));

    scheduler.documentChanged(local("second", 200));
    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([this] { return m_outcomes.back().state == SyncState::Synchronized; }));

    EXPECT_EQ(storedText(), "second");
}

TEST_F(AsyncSyncSchedulerTest, ChangesMadeWhileUploadingAreCoalescedAndRebased)
{
    std::atomic<bool> uploadStarted{ false };
    std::atomic<int> uploads{ 0 };
    m_fake.beforeSave = [&uploadStarted, &uploads]
    {
        if (uploads++ == 0)
        {
            uploadStarted = true;
            std::this_thread::sleep_for(100ms);
        }
    };

    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.documentChanged(local("A", 100));
    ASSERT_TRUE(pumpUntil([&uploadStarted] { return uploadStarted.load(); }));

    scheduler.documentChanged(local("B1", 200));
    scheduler.documentChanged(local("B2", 300));

    ASSERT_TRUE(pumpUntil([this] { return m_outcomes.size() == 2; }));
    pumpFor(100ms);

    EXPECT_EQ(m_fake.saveCalls, 2);
    EXPECT_EQ(m_outcomes.back().state, SyncState::Synchronized);
    EXPECT_EQ(storedText(), "B2");
}

TEST_F(AsyncSyncSchedulerTest, SyncNowTakesTheNewerServerDocumentAndDropsQueuedChanges)
{
    m_fake.stored = cloudDocument("theirs", 500, 600);
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.documentChanged(local("mine", 100));
    scheduler.syncNow(local("mine", 100));

    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));
    pumpFor(150ms);

    EXPECT_EQ(m_outcomes.front().state, SyncState::ServerWon);
    EXPECT_EQ(m_outcomes.front().document.text, "theirs");
    EXPECT_EQ(m_fake.saveCalls, 0);
    EXPECT_FALSE(scheduler.hasPendingWork());
}

TEST_F(AsyncSyncSchedulerTest, SyncNowUploadsANewerLocalDocument)
{
    m_fake.stored = cloudDocument("old", 100, 600);
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.syncNow(local("fresh", 500));

    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));

    EXPECT_EQ(m_outcomes.front().state, SyncState::Synchronized);
    EXPECT_EQ(storedText(), "fresh");
}

TEST_F(AsyncSyncSchedulerTest, OfflineSyncNowIsRetriedAsAReconciliation)
{
    m_fake.stored = cloudDocument("theirs", 500, 600);
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);
    m_fake.unreachable = true;

    scheduler.syncNow(local("mine", 100));
    ASSERT_TRUE(pumpUntil([this] { return sawStatus(SyncStatus::Offline); }));

    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([this] { return m_outcomes.back().state != SyncState::OfflineKeptLocal; }));

    EXPECT_EQ(m_outcomes.back().state, SyncState::ServerWon);
    EXPECT_EQ(m_outcomes.back().document.text, "theirs");
}

TEST_F(AsyncSyncSchedulerTest, UnreadableCloudDocumentIsReportedAndNotRetried)
{
    m_fake.stored = CloudDocument{ .content = nlohmann::json::object(), .updatedAt = at(5000) };
    m_fake.stored->content["unexpected"] = 1;
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
    watch(scheduler);

    scheduler.documentChanged(local("x", 10));
    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }));
    pumpFor(250ms);

    EXPECT_EQ(m_outcomes.front().state, SyncState::RemoteUnreadable);
    EXPECT_EQ(scheduler.status(), SyncStatus::Error);
    EXPECT_EQ(m_fake.saveCalls, 1);
}

TEST_F(AsyncSyncSchedulerTest, FlushUploadsWithoutWaitingForTheDebounceDelay)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, { 5000ms, 40ms, 160ms });
    watch(scheduler);

    scheduler.documentChanged(local("now", 100));
    scheduler.flush();

    ASSERT_TRUE(pumpUntil([this] { return !m_outcomes.empty(); }, 1500ms));
    EXPECT_EQ(storedText(), "now");
}

TEST_F(AsyncSyncSchedulerTest, DestroyingTheSchedulerCancelsAPendingUpload)
{
    {
        AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
        scheduler.documentChanged(local("never", 100));
    }

    pumpFor(150ms);

    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(AsyncSyncSchedulerTest, DestroyingTheSchedulerDuringAnUploadIsSafe)
{
    std::atomic<bool> uploadStarted{ false };
    m_fake.beforeSave = [&uploadStarted]
    {
        uploadStarted = true;
        std::this_thread::sleep_for(80ms);
    };
    int notifications = 0;

    {
        AsyncSyncScheduler<Entry> scheduler(m_fake, m_options);
        scheduler.signalOutcome().connect([&notifications](const stapik::sync::SyncOutcome<Entry>&) { ++notifications; });
        scheduler.signalStatusChanged().connect([&notifications](const SyncStatus) { ++notifications; });

        scheduler.documentChanged(local("late", 100));
        ASSERT_TRUE(pumpUntil([&uploadStarted] { return uploadStarted.load(); }));
        notifications = 0;
    }

    pumpFor(150ms);

    EXPECT_EQ(notifications, 0);
}

TEST(BackoffTest, DelayDoublesUpToTheMaximum)
{
    EXPECT_EQ(backoffDelay(40ms, 160ms, 0), 40ms);
    EXPECT_EQ(backoffDelay(40ms, 160ms, 1), 40ms);
    EXPECT_EQ(backoffDelay(40ms, 160ms, 2), 80ms);
    EXPECT_EQ(backoffDelay(40ms, 160ms, 3), 160ms);
    EXPECT_EQ(backoffDelay(40ms, 160ms, 4), 160ms);
    EXPECT_EQ(backoffDelay(40ms, 160ms, 50), 160ms);
}
