#include "stapik/sync/CloudSession.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/ForwardingCloudStorage.hpp"
#include "support/MainLoopPump.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using stapik::sync::AsyncSyncOptions;
    using stapik::sync::CloudSession;
    using stapik::sync::CloudSessionHooks;
    using stapik::sync::SyncEnvelope;
    using stapik::sync::SyncState;
    using stapik::sync::SyncStatus;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::ForwardingCloudStorage;
    using stapik::test::local;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;
    using stapik::test::TimePoint;

    struct PartitionEvent
    {
        std::string partition;
        SyncState state;
        std::string text;
    };

    class CloudSessionPartitionTest : public testing::Test
    {
    protected:
        CloudSessionPartitionTest()
        {
            m_fake.nextServerTime = at(1000);
            m_fake.serverTimeStep = 1s;
            m_hooks.loadConfig = [] { return std::optional<CloudStorageConfig>{ CloudStorageConfig{ .apiUrl = "https://example.com", .apiKey = "key" } }; };
            m_hooks.createStorage = [this](const CloudStorageConfig&) -> std::unique_ptr<ICloudStorage>
            {
                return std::make_unique<ForwardingCloudStorage>(m_fake);
            };
        }

        void watch(CloudSession<Entry>& session)
        {
            session.signalPartitionOutcome().connect([this](const std::string& partition, const stapik::sync::SyncOutcome<Entry>& outcome)
            {
                m_events.push_back({ partition, outcome.state, outcome.document.text });
            });
            session.signalPartitionReplaced().connect([this](const std::string& partition, const Entry& document)
            {
                m_replaced.push_back({ partition, SyncState::ServerWon, document.text });
            });
            session.signalPartitionBaselineChanged().connect([this](const std::string& partition, const TimePoint baseline)
            {
                m_baselines.emplace_back(partition, baseline);
            });
        }

        AsyncSyncOptions m_options{ 30ms, 40ms, 160ms };
        FakeCloudStorage m_fake;
        CloudSessionHooks m_hooks;
        std::vector<PartitionEvent> m_events;
        std::vector<PartitionEvent> m_replaced;
        std::vector<std::pair<std::string, TimePoint>> m_baselines;
    };
}

TEST_F(CloudSessionPartitionTest, NotConnectedSessionIgnoresPartitionRequests)
{
    CloudSessionHooks hooks;
    CloudSession<Entry> session(hooks, m_options);

    session.loadPartition("2025", local("x", 100));
    session.pushPartitionChange("2025", local("x", 100));
    session.fetchAvailableYears();
    pumpFor(100ms);

    EXPECT_EQ(m_fake.partitionLoadCalls, 0);
    EXPECT_EQ(m_fake.partitionSaveCalls, 0);
    EXPECT_FALSE(session.isPartitionLoaded("2025"));
    EXPECT_FALSE(session.hasPendingWork());
}

TEST_F(CloudSessionPartitionTest, LoadPartitionPullsTheArchivedYearAndReplacesTheLocalCopy)
{
    m_fake.partitions["2025"] = cloudDocument("year-2025", 500, 900);
    CloudSession<Entry> session(m_hooks, m_options);
    watch(session);

    session.loadPartition("2025", local("stale", 100));
    ASSERT_TRUE(pumpUntil([this] { return !m_replaced.empty(); }));

    EXPECT_EQ(m_replaced.front().partition, "2025");
    EXPECT_EQ(m_replaced.front().text, "year-2025");
    EXPECT_TRUE(session.isPartitionLoaded("2025"));
    EXPECT_EQ(m_fake.loadCalls, 0);
}

TEST_F(CloudSessionPartitionTest, LoadingAnAlreadyLoadedPartitionDoesNotTouchTheCloudAgain)
{
    m_fake.partitions["2025"] = cloudDocument("year-2025", 500, 900);
    CloudSession<Entry> session(m_hooks, m_options);
    watch(session);

    session.loadPartition("2025", local("stale", 100));
    ASSERT_TRUE(pumpUntil([&] { return session.isPartitionLoaded("2025"); }));

    session.loadPartition("2025", local("stale", 100));
    pumpFor(100ms);

    EXPECT_EQ(m_fake.partitionLoadCalls, 1);
    EXPECT_EQ(m_events.size(), 1u);
}

TEST_F(CloudSessionPartitionTest, ForgetPartitionAllowsPullingItAgain)
{
    m_fake.partitions["2025"] = cloudDocument("year-2025", 500, 900);
    CloudSession<Entry> session(m_hooks, m_options);
    watch(session);
    session.loadPartition("2025", local("stale", 100));
    ASSERT_TRUE(pumpUntil([&] { return session.isPartitionLoaded("2025"); }));

    session.forgetPartition("2025");
    EXPECT_FALSE(session.isPartitionLoaded("2025"));

    session.loadPartition("2025", local("stale", 100));
    ASSERT_TRUE(pumpUntil([this] { return m_events.size() == 2; }));
    EXPECT_EQ(m_fake.partitionLoadCalls, 2);
}

TEST_F(CloudSessionPartitionTest, OfflineLoadIsRetriedAndNotMarkedLoadedUntilItSucceeds)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 500, 900);
    m_fake.unreachable = true;
    CloudSession<Entry> session(m_hooks, m_options);
    watch(session);

    session.loadPartition("2024", local("stale", 100));
    ASSERT_TRUE(pumpUntil([this] { return !m_events.empty(); }));
    EXPECT_EQ(m_events.front().state, SyncState::OfflineKeptLocal);
    EXPECT_FALSE(session.isPartitionLoaded("2024"));

    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([&] { return session.isPartitionLoaded("2024"); }));
    EXPECT_EQ(m_replaced.back().text, "year-2024");
}

TEST_F(CloudSessionPartitionTest, PushPartitionChangeUploadsOnlyThatPartitionAndReportsTheBaseline)
{
    m_fake.partitions["2026"] = cloudDocument("v0", 10, 1000);
    m_fake.nextServerTime = at(1001);
    CloudSession<Entry> session(m_hooks, m_options);
    watch(session);

    session.pushPartitionChange("2026", local("v1", 100, 1000));
    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));

    EXPECT_EQ(m_baselines.front().first, "2026");
    EXPECT_GT(m_baselines.front().second, at(1000));
    EXPECT_EQ(m_fake.saveCalls, 0);
    EXPECT_EQ(SyncEnvelope::fromJson(m_fake.partitions.at("2026").content).payload.at("text").get<std::string>(), "v1");
}

TEST_F(CloudSessionPartitionTest, FlushSendsPendingPartitionChangeWithoutWaitingForDebounce)
{
    m_fake.partitions["2026"] = cloudDocument("v0", 10, 1000);
    AsyncSyncOptions slow{ 10s, 40ms, 160ms };
    CloudSession<Entry> session(m_hooks, slow);
    watch(session);

    session.pushPartitionChange("2026", local("v1", 100, 1000));
    EXPECT_TRUE(session.hasPendingWork());
    session.flush();

    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    EXPECT_FALSE(session.hasPendingWork());
}

TEST_F(CloudSessionPartitionTest, FetchAvailableYearsReportsSortedYears)
{
    m_fake.partitions["2025"] = cloudDocument("a", 1, 1);
    m_fake.partitions["2023"] = cloudDocument("b", 1, 1);
    CloudSession<Entry> session(m_hooks, m_options);
    std::optional<std::vector<int>> years;
    bool received = false;
    session.signalAvailableYears().connect([&](const std::optional<std::vector<int>>& value)
    {
        years = value;
        received = true;
    });

    session.fetchAvailableYears();
    ASSERT_TRUE(pumpUntil([&] { return received; }));

    ASSERT_TRUE(years.has_value());
    EXPECT_EQ(*years, (std::vector<int>{ 2023, 2025 }));
}

TEST_F(CloudSessionPartitionTest, FetchAvailableYearsReportsUnavailableWhenOffline)
{
    m_fake.unreachable = true;
    CloudSession<Entry> session(m_hooks, m_options);
    std::optional<std::vector<int>> years{ std::vector<int>{ 1 } };
    bool received = false;
    session.signalAvailableYears().connect([&](const std::optional<std::vector<int>>& value)
    {
        years = value;
        received = true;
    });

    session.fetchAvailableYears();
    ASSERT_TRUE(pumpUntil([&] { return received; }));

    EXPECT_FALSE(years.has_value());
}
