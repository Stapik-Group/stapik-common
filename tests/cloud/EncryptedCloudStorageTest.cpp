#include "stapik/cloud/EncryptedCloudStorage.hpp"

#include "stapik/cloud/CompressedBlob.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/ForwardingCloudStorage.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using stapik::cloud::EncryptedCloudStorage;
    using stapik::crypto::PayloadCipher;
    using stapik::test::FakeCloudStorage;
    using stapik::test::ForwardingCloudStorage;
    using TimePoint = std::chrono::system_clock::time_point;

    constexpr auto PASSPHRASE = "correct horse battery staple";
    constexpr auto SCOPE = "budget";
    constexpr auto SECRET_TITLE = "recognizable secret title";

    nlohmann::json documentWithTitle(const std::string& title)
    {
        auto document = nlohmann::json::object();
        document["title"] = title;
        return document;
    }

    class EncryptedCloudStorageTest : public testing::Test
    {
    protected:
        EncryptedCloudStorageTest()
        {
            m_server.nextServerTime = TimePoint{} + std::chrono::hours(1);
        }

        [[nodiscard]] std::unique_ptr<EncryptedCloudStorage> storage(const std::string& passphrase = PASSPHRASE, const std::string& scope = SCOPE)
        {
            return std::make_unique<EncryptedCloudStorage>(std::make_unique<ForwardingCloudStorage>(m_server), passphrase, scope);
        }

        FakeCloudStorage m_server;
    };
}

TEST_F(EncryptedCloudStorageTest, ServerReceivesOnlyEncryptedBlob)
{
    [[maybe_unused]] const auto result = storage()->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    ASSERT_TRUE(m_server.stored.has_value());
    EXPECT_TRUE(PayloadCipher::isEncrypted(m_server.stored->content));
    EXPECT_EQ(m_server.stored->content.dump().find("recognizable"), std::string::npos);
}

TEST_F(EncryptedCloudStorageTest, LoadReturnsTheSavedDocument)
{
    const auto cloud = storage();
    const auto written = cloud->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    const auto loaded = cloud->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, documentWithTitle(SECRET_TITLE));
    EXPECT_EQ(loaded->updatedAt, written.document.updatedAt);
}

TEST_F(EncryptedCloudStorageTest, DocumentSavedOnOneDeviceLoadsOnAnother)
{
    [[maybe_unused]] const auto result = storage()->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    const auto loaded = storage()->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, documentWithTitle(SECRET_TITLE));
}

TEST_F(EncryptedCloudStorageTest, SaveReturnsThePlainDocumentWithServerTime)
{
    const auto result = storage()->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    EXPECT_FALSE(result.conflict);
    EXPECT_EQ(result.document.content, documentWithTitle(SECRET_TITLE));
    EXPECT_EQ(result.document.updatedAt, m_server.stored->updatedAt);
}

TEST_F(EncryptedCloudStorageTest, LoadOfMissingDocumentReturnsNothing)
{
    EXPECT_FALSE(storage()->loadDocument().has_value());
}

TEST_F(EncryptedCloudStorageTest, EmptyServerDocumentIsReturnedAsIs)
{
    m_server.stored = CloudDocument{ .content = nlohmann::json::object(), .updatedAt = m_server.nextServerTime };

    const auto loaded = storage()->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->content.empty());
}

TEST_F(EncryptedCloudStorageTest, LoadWithWrongPassphraseThrowsDecryptionException)
{
    [[maybe_unused]] const auto result = storage()->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage("other passphrase")->loadDocument(), CloudDecryptionException);
}

TEST_F(EncryptedCloudStorageTest, ModifiedServerDataIsRejected)
{
    [[maybe_unused]] const auto result = storage()->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});
    auto ciphertext = m_server.stored->content.at("ciphertext").get<std::string>();
    ciphertext[2] = ciphertext[2] == 'A' ? 'B' : 'A';
    m_server.stored->content["ciphertext"] = ciphertext;

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage()->loadDocument(), CloudDecryptionException);
}

TEST_F(EncryptedCloudStorageTest, DocumentSavedBeforeEncryptionIsReturnedUnchanged)
{
    m_server.stored = CloudDocument{ .content = documentWithTitle("old plain text"), .updatedAt = m_server.nextServerTime };

    const auto loaded = storage()->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, documentWithTitle("old plain text"));
}

TEST_F(EncryptedCloudStorageTest, ConflictReturnsTheDecryptedServerDocument)
{
    [[maybe_unused]] const auto first = storage()->saveDocument(documentWithTitle("from the other device"), TimePoint{});

    const auto second = storage()->saveDocument(documentWithTitle("from this device"), TimePoint{});

    EXPECT_TRUE(second.conflict);
    EXPECT_EQ(second.document.content, documentWithTitle("from the other device"));
    EXPECT_EQ(second.document.updatedAt, m_server.stored->updatedAt);
}

TEST_F(EncryptedCloudStorageTest, ConflictWithWrongPassphraseThrowsDecryptionException)
{
    [[maybe_unused]] const auto first = storage()->saveDocument(documentWithTitle("from the other device"), TimePoint{});

    EXPECT_THROW([[maybe_unused]] const auto second = storage("other passphrase")->saveDocument(documentWithTitle("from this device"), TimePoint{}), CloudDecryptionException);
    EXPECT_TRUE(PayloadCipher::isEncrypted(m_server.stored->content));
}

TEST_F(EncryptedCloudStorageTest, PartitionsAreEncryptedAndRoundTrip)
{
    const auto cloud = storage();

    [[maybe_unused]] const auto written = cloud->savePartition("2025", documentWithTitle(SECRET_TITLE), TimePoint{});
    const auto loaded = cloud->loadPartition("2025");

    ASSERT_TRUE(m_server.partitions.contains("2025"));
    EXPECT_TRUE(PayloadCipher::isEncrypted(m_server.partitions.at("2025").content));
    EXPECT_EQ(m_server.partitions.at("2025").content.dump().find("recognizable"), std::string::npos);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, documentWithTitle(SECRET_TITLE));
}

TEST_F(EncryptedCloudStorageTest, LoadOfMissingPartitionReturnsNothing)
{
    EXPECT_FALSE(storage()->loadPartition("2025").has_value());
}

TEST_F(EncryptedCloudStorageTest, PartitionConflictReturnsTheDecryptedServerPartition)
{
    [[maybe_unused]] const auto first = storage()->savePartition("2025", documentWithTitle("from the other device"), TimePoint{});

    const auto second = storage()->savePartition("2025", documentWithTitle("from this device"), TimePoint{});

    EXPECT_TRUE(second.conflict);
    EXPECT_EQ(second.document.content, documentWithTitle("from the other device"));
}

TEST_F(EncryptedCloudStorageTest, PartitionCannotBeSwappedForAnotherPartition)
{
    const auto cloud = storage();
    [[maybe_unused]] const auto written = cloud->savePartition("2025", documentWithTitle(SECRET_TITLE), TimePoint{});
    m_server.partitions["2024"] = m_server.partitions.at("2025");

    EXPECT_THROW([[maybe_unused]] const auto loaded = cloud->loadPartition("2024"), CloudDecryptionException);
}

TEST_F(EncryptedCloudStorageTest, MainDocumentCannotBePassedOffAsPartition)
{
    const auto cloud = storage();
    [[maybe_unused]] const auto written = cloud->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});
    m_server.partitions["2025"] = *m_server.stored;

    EXPECT_THROW([[maybe_unused]] const auto loaded = cloud->loadPartition("2025"), CloudDecryptionException);
}

TEST_F(EncryptedCloudStorageTest, DocumentOfAnotherSlotCannotBeRead)
{
    [[maybe_unused]] const auto written = storage(PASSPHRASE, "groceries")->saveDocument(documentWithTitle(SECRET_TITLE), TimePoint{});

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage(PASSPHRASE, "budget")->loadDocument(), CloudDecryptionException);
}

TEST_F(EncryptedCloudStorageTest, ListingAndDeletingPartitionsGoThroughToTheServer)
{
    const auto cloud = storage();
    [[maybe_unused]] const auto first = cloud->savePartition("2024", documentWithTitle("a"), TimePoint{});
    [[maybe_unused]] const auto second = cloud->savePartition("2025", documentWithTitle("b"), TimePoint{});

    const auto listed = cloud->listPartitions();

    ASSERT_EQ(listed.size(), 2U);
    EXPECT_EQ(listed.front().partition, "2024");
    EXPECT_TRUE(cloud->deletePartition("2024"));
    EXPECT_FALSE(cloud->deletePartition("2024"));
    EXPECT_FALSE(m_server.partitions.contains("2024"));
}

TEST_F(EncryptedCloudStorageTest, ChangeFeedGoesThroughToTheServerUntouched)
{
    m_server.changes = { CloudChange{ .slotKey = "budget", .partition = "2025", .contentHash = "h", .updatedAt = TimePoint{} + std::chrono::seconds{ 50 } } };
    m_server.changesServerTime = TimePoint{} + std::chrono::seconds{ 60 };

    const auto list = storage()->listChanges(TimePoint{} + std::chrono::seconds{ 10 });

    EXPECT_EQ(list.serverTime, m_server.changesServerTime);
    ASSERT_EQ(list.changes.size(), 1U);
    EXPECT_EQ(list.changes.front().partition, std::optional<std::string>{ "2025" });
    EXPECT_EQ(m_server.changeQueries, std::vector<TimePoint>{ TimePoint{} + std::chrono::seconds{ 10 } });
}

TEST_F(EncryptedCloudStorageTest, InvalidUtf8IsReplacedInsteadOfFailingTheSave)
{
    const auto cloud = storage();

    [[maybe_unused]] const auto result = cloud->saveDocument(documentWithTitle("bad\xff title"), TimePoint{});
    const auto loaded = cloud->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, documentWithTitle("bad\xEF\xBF\xBD title"));
}

TEST_F(EncryptedCloudStorageTest, CompressedBlobIsEncryptedAsRawGzipStream)
{
    const auto blob = stapik::cloud::compressedBlobOf(std::string("\x1f\x8b\x08\x00\xff\xfe\x00stream", 14));

    [[maybe_unused]] const auto result = storage()->saveDocument(blob, TimePoint{});

    ASSERT_TRUE(m_server.stored.has_value());
    EXPECT_TRUE(PayloadCipher::isEncrypted(m_server.stored->content));
    EXPECT_EQ(m_server.stored->content.at("version"), 2);
}

TEST_F(EncryptedCloudStorageTest, CompressedBlobComesBackUnchanged)
{
    const auto blob = stapik::cloud::compressedBlobOf(std::string("\x1f\x8b\x08\x00\xff\xfe\x00stream", 14));
    const auto cloud = storage();

    [[maybe_unused]] const auto result = cloud->saveDocument(blob, TimePoint{});
    const auto loaded = cloud->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, blob);
}

TEST_F(EncryptedCloudStorageTest, DocumentThatOnlyLooksCompressedIsEncryptedAsText)
{
    auto lookalike = documentWithTitle("not a real blob");
    lookalike["format"] = "stapik-gzip";
    lookalike["version"] = 7;
    lookalike["data"] = "QQ==";
    const auto cloud = storage();

    [[maybe_unused]] const auto result = cloud->saveDocument(lookalike, TimePoint{});
    const auto loaded = cloud->loadDocument();

    EXPECT_EQ(m_server.stored->content.at("version"), 1);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, lookalike);
}

TEST_F(EncryptedCloudStorageTest, CompressedPartitionBlobRoundTrips)
{
    const auto blob = stapik::cloud::compressedBlobOf("partition stream");
    const auto cloud = storage();

    [[maybe_unused]] const auto result = cloud->savePartition("2025", blob, TimePoint{});
    const auto loaded = cloud->loadPartition("2025");

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, blob);
}
