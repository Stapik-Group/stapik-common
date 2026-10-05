#include "stapik/cloud/CloudStorageException.hpp"
#include "stapik/cloud/ICloudStorage.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <tuple>

namespace
{
    using stapik::test::at;
    using stapik::test::FakeCloudStorage;

    class MainDocumentOnlyStorage final : public ICloudStorage
    {
    public:
        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override { return std::nullopt; }

        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json&, std::chrono::system_clock::time_point) const override
        {
            return {};
        }
    };

    class CloudPartitionsTest : public testing::Test
    {
    protected:
        CloudPartitionsTest()
        {
            m_fake.nextServerTime = at(1000);
            m_fake.serverTimeStep = std::chrono::seconds(10);
        }

        FakeCloudStorage m_fake;
    };
}

TEST(CloudPartitionsDefaultsTest, StorageWithoutPartitionSupportRejectsEveryPartitionCall)
{
    const MainDocumentOnlyStorage storage;

    EXPECT_THROW(std::ignore = storage.listPartitions(), CloudStorageException);
    EXPECT_THROW(std::ignore = storage.loadPartition("2025"), CloudStorageException);
    EXPECT_THROW(std::ignore = storage.savePartition("2025", nlohmann::json::object(), at(0)), CloudStorageException);
    EXPECT_THROW(std::ignore = storage.deletePartition("2025"), CloudStorageException);
}

TEST_F(CloudPartitionsTest, MissingPartitionLoadsAsNullopt)
{
    EXPECT_FALSE(m_fake.loadPartition("2025").has_value());
}

TEST_F(CloudPartitionsTest, FirstWriteCreatesThePartition)
{
    const auto result = m_fake.savePartition("2025", nlohmann::json{ { "a", 1 } }, at(0));

    EXPECT_FALSE(result.conflict);
    EXPECT_EQ(result.document.updatedAt, at(1000));

    const auto loaded = m_fake.loadPartition("2025");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, (nlohmann::json{ { "a", 1 } }));
}

TEST_F(CloudPartitionsTest, WriteAgainstStaleBaselineReportsConflictAndKeepsServerCopy)
{
    std::ignore = m_fake.savePartition("2025", nlohmann::json{ { "v", 1 } }, at(0));

    const auto result = m_fake.savePartition("2025", nlohmann::json{ { "v", 2 } }, at(500));

    EXPECT_TRUE(result.conflict);
    EXPECT_EQ(result.document.content, (nlohmann::json{ { "v", 1 } }));
    EXPECT_EQ(m_fake.loadPartition("2025")->content, (nlohmann::json{ { "v", 1 } }));
}

TEST_F(CloudPartitionsTest, WriteAgainstCurrentBaselineSucceeds)
{
    const auto first = m_fake.savePartition("2025", nlohmann::json{ { "v", 1 } }, at(0));

    const auto second = m_fake.savePartition("2025", nlohmann::json{ { "v", 2 } }, first.document.updatedAt);

    EXPECT_FALSE(second.conflict);
    EXPECT_GT(second.document.updatedAt, first.document.updatedAt);
}

TEST_F(CloudPartitionsTest, PartitionsAreIndependentFromEachOtherAndFromTheMainDocument)
{
    std::ignore = m_fake.savePartition("2024", nlohmann::json{ { "y", 2024 } }, at(0));
    std::ignore = m_fake.savePartition("2025", nlohmann::json{ { "y", 2025 } }, at(0));

    EXPECT_FALSE(m_fake.stored.has_value());
    EXPECT_EQ(m_fake.loadPartition("2024")->content, (nlohmann::json{ { "y", 2024 } }));
    EXPECT_EQ(m_fake.loadPartition("2025")->content, (nlohmann::json{ { "y", 2025 } }));
}

TEST_F(CloudPartitionsTest, ListingReturnsPartitionsSortedByKeyWithoutTheMainDocument)
{
    m_fake.stored = CloudDocument{ .content = nlohmann::json::object(), .updatedAt = at(1) };
    std::ignore = m_fake.savePartition("2025", nlohmann::json{ { "y", 2025 } }, at(0));
    std::ignore = m_fake.savePartition("2024", nlohmann::json{ { "y", 2024 } }, at(0));

    const auto listing = m_fake.listPartitions();

    ASSERT_EQ(listing.size(), 2u);
    EXPECT_EQ(listing[0].partition, "2024");
    EXPECT_EQ(listing[1].partition, "2025");
    EXPECT_GT(listing[0].sizeBytes, 0);
    EXPECT_FALSE(listing[0].contentHash.empty());
}

TEST_F(CloudPartitionsTest, DeleteRemovesPartitionAndReportsWhetherAnythingWasDeleted)
{
    std::ignore = m_fake.savePartition("2025", nlohmann::json::object(), at(0));

    EXPECT_TRUE(m_fake.deletePartition("2025"));
    EXPECT_FALSE(m_fake.deletePartition("2025"));
    EXPECT_FALSE(m_fake.loadPartition("2025").has_value());
}

TEST_F(CloudPartitionsTest, UnreachableStorageThrowsOnEveryPartitionCall)
{
    m_fake.unreachable = true;

    EXPECT_THROW(std::ignore = m_fake.listPartitions(), CloudStorageException);
    EXPECT_THROW(std::ignore = m_fake.loadPartition("2025"), CloudStorageException);
    EXPECT_THROW(std::ignore = m_fake.savePartition("2025", nlohmann::json::object(), at(0)), CloudStorageException);
    EXPECT_THROW(m_fake.deletePartition("2025"), CloudStorageException);
}
