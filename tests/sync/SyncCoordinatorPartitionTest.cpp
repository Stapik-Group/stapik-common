#include "stapik/sync/SyncCoordinator.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace
{
    using stapik::sync::SyncCoordinator;
    using stapik::sync::SyncState;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::TimePoint;

    class SyncCoordinatorPartitionTest : public testing::Test
    {
    protected:
        SyncCoordinatorPartitionTest()
        {
            m_fake.nextServerTime = at(1000);
        }

        FakeCloudStorage m_fake;
        SyncCoordinator<Entry> m_coordinator{ m_fake, std::string("2025") };
    };
}

TEST_F(SyncCoordinatorPartitionTest, ExposesThePartitionItWorksOn)
{
    EXPECT_EQ(m_coordinator.partition(), std::optional<std::string>{ "2025" });
    EXPECT_FALSE(SyncCoordinator<Entry>{ m_fake }.partition().has_value());
}

TEST_F(SyncCoordinatorPartitionTest, ConnectWithoutPartitionUploadsLocalToThePartitionOnly)
{
    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(1000) });
    ASSERT_TRUE(m_fake.partitions.contains("2025"));
    EXPECT_FALSE(m_fake.stored.has_value());
    EXPECT_EQ(m_fake.loadCalls, 0);
    EXPECT_EQ(m_fake.saveCalls, 0);
    EXPECT_EQ(m_fake.partitionLoadCalls, 1);
    EXPECT_EQ(m_fake.partitionSaveCalls, 1);
}

TEST_F(SyncCoordinatorPartitionTest, ConnectPullsNewerPartitionFromTheCloud)
{
    m_fake.partitions["2025"] = cloudDocument("remote", 50, 500);

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::ServerWon);
    EXPECT_EQ(outcome.document.text, "remote");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(500) });
    EXPECT_EQ(m_fake.partitionSaveCalls, 0);
}

TEST_F(SyncCoordinatorPartitionTest, ConnectWithEqualContentTimeOnlyAdoptsTheBaseline)
{
    m_fake.partitions["2025"] = cloudDocument("same", 10, 500);

    const auto outcome = m_coordinator.resolveOnConnect(local("same", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(500) });
    EXPECT_EQ(m_fake.partitionSaveCalls, 0);
}

TEST_F(SyncCoordinatorPartitionTest, ConnectWithNewerLocalUploadsAgainstTheRemoteVersion)
{
    m_fake.partitions["2025"] = cloudDocument("old", 5, 500);

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.partitionSaveCalls, 1);
    EXPECT_EQ(m_fake.partitions.at("2025").updatedAt, at(1000));
}

TEST_F(SyncCoordinatorPartitionTest, PushWithKnownBaselineSucceeds)
{
    m_fake.partitions["2025"] = cloudDocument("v1", 10, 1000);

    const auto outcome = m_coordinator.pushLocalChange(local("v2", 20, 1000));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.partitionSaveCalls, 1);
}

TEST_F(SyncCoordinatorPartitionTest, PushLosingTheRaceAcceptsTheServerPartition)
{
    m_fake.partitions["2025"] = cloudDocument("theirs", 30, 2000);

    const auto outcome = m_coordinator.pushLocalChange(local("mine", 20, 1000));

    EXPECT_EQ(outcome.state, SyncState::LostRaceAcceptedServer);
    EXPECT_EQ(outcome.document.text, "theirs");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(2000) });
}

TEST_F(SyncCoordinatorPartitionTest, UnreachableCloudKeepsTheLocalDocument)
{
    m_fake.unreachable = true;

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::OfflineKeptLocal);
    EXPECT_EQ(outcome.document.text, "mine");
}

TEST_F(SyncCoordinatorPartitionTest, MainDocumentCoordinatorNeverTouchesPartitions)
{
    m_fake.partitions["2025"] = cloudDocument("archived", 50, 500);
    SyncCoordinator<Entry> mainCoordinator{ m_fake };

    const auto outcome = mainCoordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    ASSERT_TRUE(m_fake.stored.has_value());
    EXPECT_EQ(m_fake.partitionLoadCalls, 0);
    EXPECT_EQ(m_fake.partitionSaveCalls, 0);
}

TEST_F(SyncCoordinatorPartitionTest, PartitionsSyncIndependently)
{
    SyncCoordinator<Entry> previousYear{ m_fake, std::string("2024") };
    m_fake.partitions["2024"] = cloudDocument("year-2024", 50, 500);

    const auto current = m_coordinator.resolveOnConnect(local("year-2025", 10));
    const auto previous = previousYear.resolveOnConnect(local("stale", 10));

    EXPECT_EQ(current.state, SyncState::Synchronized);
    EXPECT_EQ(previous.state, SyncState::ServerWon);
    EXPECT_EQ(previous.document.text, "year-2024");
    EXPECT_EQ(m_fake.partitions.size(), 2u);
}
