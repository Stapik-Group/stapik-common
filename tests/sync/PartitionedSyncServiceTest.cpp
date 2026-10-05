#include "stapik/sync/PartitionedSyncService.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using stapik::sync::PartitionedSyncService;
    using stapik::sync::SyncState;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::TimePoint;

    class PartitionedSyncServiceTest : public testing::Test
    {
    protected:
        PartitionedSyncServiceTest()
        {
            m_fake.nextServerTime = at(1000);
        }

        FakeCloudStorage m_fake;
        PartitionedSyncService<Entry> m_service{ m_fake };
    };
}

TEST_F(PartitionedSyncServiceTest, ResolveTouchesOnlyTheRequestedPartition)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 50, 500);
    m_fake.partitions["2025"] = cloudDocument("year-2025", 50, 600);

    const auto outcome = m_service.resolve("2025", local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::ServerWon);
    EXPECT_EQ(outcome.document.text, "year-2025");
    EXPECT_EQ(m_fake.loadedPartitions, std::vector<std::string>{ "2025" });
    EXPECT_EQ(m_fake.loadCalls, 0);
}

TEST_F(PartitionedSyncServiceTest, ResolveIfNotLoadedPullsOnlyOnce)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 50, 500);

    const auto first = m_service.resolveIfNotLoaded("2024", local("stale", 10));
    const auto second = m_service.resolveIfNotLoaded("2024", local("stale", 10));

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->state, SyncState::ServerWon);
    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(m_fake.partitionLoadCalls, 1);
    EXPECT_TRUE(m_service.isLoaded("2024"));
}

TEST_F(PartitionedSyncServiceTest, OfflineAttemptDoesNotMarkThePartitionLoaded)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 50, 500);
    m_fake.unreachable = true;

    const auto offline = m_service.resolveIfNotLoaded("2024", local("stale", 10));

    ASSERT_TRUE(offline.has_value());
    EXPECT_EQ(offline->state, SyncState::OfflineKeptLocal);
    EXPECT_FALSE(m_service.isLoaded("2024"));

    m_fake.unreachable = false;
    const auto retried = m_service.resolveIfNotLoaded("2024", local("stale", 10));

    ASSERT_TRUE(retried.has_value());
    EXPECT_EQ(retried->state, SyncState::ServerWon);
    EXPECT_TRUE(m_service.isLoaded("2024"));
}

TEST_F(PartitionedSyncServiceTest, UploadingAFreshPartitionMarksItLoaded)
{
    const auto outcome = m_service.resolve("2026", local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_TRUE(m_service.isLoaded("2026"));
    EXPECT_TRUE(m_fake.partitions.contains("2026"));
}

TEST_F(PartitionedSyncServiceTest, PushUsesTheDocumentBaselineAndTheRequestedPartition)
{
    m_fake.partitions["2026"] = cloudDocument("v1", 10, 1000);

    const auto outcome = m_service.push("2026", local("v2", 20, 1000));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.savedPartitions, std::vector<std::string>{ "2026" });
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(PartitionedSyncServiceTest, ForgetAndResetAllowLoadingAgain)
{
    m_fake.partitions["2024"] = cloudDocument("year-2024", 50, 500);
    m_fake.partitions["2025"] = cloudDocument("year-2025", 50, 600);
    std::ignore = m_service.resolve("2024", local("a", 10));
    std::ignore = m_service.resolve("2025", local("b", 10));

    m_service.forget("2024");
    EXPECT_FALSE(m_service.isLoaded("2024"));
    EXPECT_TRUE(m_service.isLoaded("2025"));

    m_service.reset();
    EXPECT_FALSE(m_service.isLoaded("2025"));
}

TEST_F(PartitionedSyncServiceTest, AvailableYearsAreSortedAndIgnoreNonYearKeys)
{
    m_fake.partitions["2025"] = cloudDocument("a", 1, 1);
    m_fake.partitions["2023"] = cloudDocument("b", 1, 1);
    m_fake.partitions["notes"] = cloudDocument("c", 1, 1);

    const auto years = m_service.availableYears();

    ASSERT_TRUE(years.has_value());
    EXPECT_EQ(*years, (std::vector<int>{ 2023, 2025 }));
}

TEST_F(PartitionedSyncServiceTest, AvailablePartitionsAreEmptyWhenTheCloudHasNone)
{
    const auto keys = m_service.availablePartitions();

    ASSERT_TRUE(keys.has_value());
    EXPECT_TRUE(keys->empty());
}

TEST_F(PartitionedSyncServiceTest, ListingIsUnavailableWhenTheCloudIsUnreachable)
{
    m_fake.unreachable = true;

    EXPECT_FALSE(m_service.availablePartitions().has_value());
    EXPECT_FALSE(m_service.availableYears().has_value());
}
