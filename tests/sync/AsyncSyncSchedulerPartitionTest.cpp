#include "stapik/sync/AsyncSyncScheduler.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/MainLoopPump.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using stapik::sync::AsyncSyncOptions;
    using stapik::sync::AsyncSyncScheduler;
    using stapik::sync::SyncEnvelope;
    using stapik::sync::SyncOutcome;
    using stapik::sync::SyncState;
    using stapik::sync::SyncStatus;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::pumpUntil;
    using stapik::test::TimePoint;

    struct Recorder
    {
        std::vector<SyncState> states;
        std::vector<SyncStatus> statuses;
        std::vector<Entry> documents;

        void attach(AsyncSyncScheduler<Entry>& scheduler)
        {
            scheduler.signalStatusChanged().connect([this](const SyncStatus status) { statuses.push_back(status); });
            scheduler.signalOutcome().connect([this](const SyncOutcome<Entry>& outcome)
            {
                states.push_back(outcome.state);
                documents.push_back(outcome.document);
            });
        }

        [[nodiscard]] bool sawStatus(const SyncStatus status) const
        {
            return std::ranges::find(statuses, status) != statuses.end();
        }
    };

    class AsyncSyncSchedulerPartitionTest : public testing::Test
    {
    protected:
        AsyncSyncSchedulerPartitionTest()
        {
            m_fake.nextServerTime = at(1000);
            m_fake.serverTimeStep = 1s;
        }

        AsyncSyncOptions m_options{ 30ms, 40ms, 160ms };
        FakeCloudStorage m_fake;
    };
}

TEST_F(AsyncSyncSchedulerPartitionTest, ExposesThePartitionItSyncs)
{
    const AsyncSyncScheduler<Entry> partitioned(m_fake, m_options, std::string("2025"));
    const AsyncSyncScheduler<Entry> main(m_fake, m_options);

    EXPECT_EQ(partitioned.partition(), std::optional<std::string>{ "2025" });
    EXPECT_FALSE(main.partition().has_value());
}

TEST_F(AsyncSyncSchedulerPartitionTest, SyncNowUploadsToThePartitionAndLeavesTheMainDocumentAlone)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options, std::string("2025"));
    Recorder recorder;
    recorder.attach(scheduler);

    scheduler.syncNow(local("a", 100));
    ASSERT_TRUE(pumpUntil([&] { return !recorder.states.empty(); }));

    EXPECT_EQ(recorder.states.front(), SyncState::Synchronized);
    EXPECT_EQ(recorder.documents.front().cloudBaseline, std::optional<TimePoint>{ at(1000) });
    EXPECT_TRUE(m_fake.partitions.contains("2025"));
    EXPECT_FALSE(m_fake.stored.has_value());
    EXPECT_EQ(m_fake.loadCalls, 0);
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(AsyncSyncSchedulerPartitionTest, SyncNowPullsANewerPartition)
{
    m_fake.partitions["2025"] = cloudDocument("remote", 500, 900);
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options, std::string("2025"));
    Recorder recorder;
    recorder.attach(scheduler);

    scheduler.syncNow(local("mine", 100));
    ASSERT_TRUE(pumpUntil([&] { return !recorder.states.empty(); }));

    EXPECT_EQ(recorder.states.front(), SyncState::ServerWon);
    EXPECT_EQ(recorder.documents.front().text, "remote");
    EXPECT_EQ(m_fake.partitionSaveCalls, 0);
}

TEST_F(AsyncSyncSchedulerPartitionTest, ChangesAreDebouncedAndPushedToThePartition)
{
    m_fake.partitions["2025"] = cloudDocument("v0", 10, 1000);
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options, std::string("2025"));
    Recorder recorder;
    recorder.attach(scheduler);

    for (int index = 1; index <= 4; ++index)
        scheduler.documentChanged(local("v" + std::to_string(index), 100 + index, 1000));

    ASSERT_TRUE(pumpUntil([&] { return !recorder.states.empty(); }));

    EXPECT_EQ(recorder.states.front(), SyncState::Synchronized);
    EXPECT_EQ(m_fake.partitionSaveCalls, 1);
    EXPECT_EQ(SyncEnvelope::fromJson(m_fake.partitions.at("2025").content).payload.at("text").get<std::string>(), "v4");
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(AsyncSyncSchedulerPartitionTest, OfflinePartitionPushIsRetriedUntilItSucceeds)
{
    AsyncSyncScheduler<Entry> scheduler(m_fake, m_options, std::string("2024"));
    Recorder recorder;
    recorder.attach(scheduler);
    m_fake.unreachable = true;

    scheduler.documentChanged(local("offline edit", 100));
    ASSERT_TRUE(pumpUntil([&] { return recorder.sawStatus(SyncStatus::Offline); }));
    EXPECT_TRUE(scheduler.hasPendingWork());

    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([&] { return recorder.states.back() == SyncState::Synchronized; }));

    EXPECT_EQ(scheduler.status(), SyncStatus::Idle);
    EXPECT_TRUE(m_fake.partitions.contains("2024"));
}

TEST_F(AsyncSyncSchedulerPartitionTest, SchedulersOnDifferentPartitionsDoNotInterfere)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 500, 900);
    AsyncSyncScheduler<Entry> previousYear(m_fake, m_options, std::string("2024"));
    AsyncSyncScheduler<Entry> currentYear(m_fake, m_options, std::string("2025"));
    Recorder previous;
    Recorder current;
    previous.attach(previousYear);
    current.attach(currentYear);

    previousYear.syncNow(local("stale", 100));
    ASSERT_TRUE(pumpUntil([&] { return !previous.states.empty(); }));

    currentYear.syncNow(local("year-2025", 100));
    ASSERT_TRUE(pumpUntil([&] { return !current.states.empty(); }));

    EXPECT_EQ(previous.states.front(), SyncState::ServerWon);
    EXPECT_EQ(previous.documents.front().text, "year-2024");
    EXPECT_EQ(current.states.front(), SyncState::Synchronized);
    EXPECT_EQ(m_fake.partitions.size(), 2u);
}
