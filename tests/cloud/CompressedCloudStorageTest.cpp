#include "stapik/cloud/CompressedCloudStorage.hpp"

#include "stapik/cloud/EncryptedCloudStorage.hpp"
#include "stapik/compression/Gzip.hpp"
#include "stapik/text/JsonText.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/ForwardingCloudStorage.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <random>
#include <string>

namespace
{
    using stapik::cloud::CompressedCloudStorage;
    using stapik::cloud::EncryptedCloudStorage;
    using stapik::test::FakeCloudStorage;
    using stapik::test::ForwardingCloudStorage;
    using TimePoint = std::chrono::system_clock::time_point;

    constexpr std::size_t THRESHOLD = 1024;

    nlohmann::json smallDocument()
    {
        auto document = nlohmann::json::object();
        document["title"] = "small";
        return document;
    }

    nlohmann::json largeDocument()
    {
        auto entries = nlohmann::json::array();
        for (int index = 0; index < 200; ++index)
        {
            auto entry = nlohmann::json::object();
            entry["title"] = "Groceries";
            entry["amount"] = index;
            entries.push_back(entry);
        }

        auto document = nlohmann::json::object();
        document["entries"] = entries;
        return document;
    }

    std::string incompressibleText(const std::size_t length)
    {
        std::string alphabet;
        for (char symbol = '!'; symbol <= '~'; ++symbol)
            if (symbol != '"' && symbol != '\\')
                alphabet.push_back(symbol);

        std::mt19937 generator{ 12345 };
        std::uniform_int_distribution<std::size_t> pick{ 0, alphabet.size() - 1 };

        std::string text;
        text.reserve(length);
        for (std::size_t index = 0; index < length; ++index)
            text.push_back(alphabet[pick(generator)]);
        return text;
    }

    class CompressedCloudStorageTest : public testing::Test
    {
    protected:
        CompressedCloudStorageTest()
        {
            m_server.nextServerTime = TimePoint{} + std::chrono::hours(1);
        }

        [[nodiscard]] std::unique_ptr<CompressedCloudStorage> storage(const std::size_t threshold = THRESHOLD)
        {
            return std::make_unique<CompressedCloudStorage>(std::make_unique<ForwardingCloudStorage>(m_server), threshold);
        }

        FakeCloudStorage m_server;
    };
}

TEST_F(CompressedCloudStorageTest, LargeDocumentIsSentCompressed)
{
    const auto document = largeDocument();

    [[maybe_unused]] const auto result = storage()->saveDocument(document, TimePoint{});

    ASSERT_TRUE(m_server.stored.has_value());
    EXPECT_TRUE(CompressedCloudStorage::isCompressed(m_server.stored->content));
    EXPECT_LT(m_server.stored->content.dump().size() * 2, document.dump().size());
}

TEST_F(CompressedCloudStorageTest, SmallDocumentIsSentUntouched)
{
    [[maybe_unused]] const auto result = storage()->saveDocument(smallDocument(), TimePoint{});

    ASSERT_TRUE(m_server.stored.has_value());
    EXPECT_EQ(m_server.stored->content, smallDocument());
}

TEST_F(CompressedCloudStorageTest, DocumentThatDoesNotShrinkIsSentUntouched)
{
    auto noise = nlohmann::json::object();
    noise["text"] = incompressibleText(600);

    [[maybe_unused]] const auto result = storage(64)->saveDocument(noise, TimePoint{});

    EXPECT_EQ(m_server.stored->content, noise);
}

TEST_F(CompressedCloudStorageTest, LoadReturnsTheSavedLargeDocument)
{
    const auto cloud = storage();
    const auto written = cloud->saveDocument(largeDocument(), TimePoint{});

    const auto loaded = cloud->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, largeDocument());
    EXPECT_EQ(loaded->updatedAt, written.document.updatedAt);
}

TEST_F(CompressedCloudStorageTest, SaveReturnsThePlainDocument)
{
    const auto result = storage()->saveDocument(largeDocument(), TimePoint{});

    EXPECT_FALSE(result.conflict);
    EXPECT_EQ(result.document.content, largeDocument());
}

TEST_F(CompressedCloudStorageTest, DocumentSavedWithoutCompressionStillLoads)
{
    m_server.stored = CloudDocument{ .content = smallDocument(), .updatedAt = m_server.nextServerTime };

    const auto loaded = storage()->loadDocument();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, smallDocument());
}

TEST_F(CompressedCloudStorageTest, LoadOfMissingDocumentReturnsNothing)
{
    EXPECT_FALSE(storage()->loadDocument().has_value());
}

TEST_F(CompressedCloudStorageTest, ConflictReturnsTheDecompressedServerDocument)
{
    [[maybe_unused]] const auto first = storage()->saveDocument(largeDocument(), TimePoint{});

    const auto second = storage()->saveDocument(smallDocument(), TimePoint{});

    EXPECT_TRUE(second.conflict);
    EXPECT_EQ(second.document.content, largeDocument());
}

TEST_F(CompressedCloudStorageTest, PartitionsAreCompressedToo)
{
    const auto cloud = storage();

    [[maybe_unused]] const auto written = cloud->savePartition("2025", largeDocument(), TimePoint{});
    const auto loaded = cloud->loadPartition("2025");

    EXPECT_TRUE(CompressedCloudStorage::isCompressed(m_server.partitions.at("2025").content));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, largeDocument());
}

TEST_F(CompressedCloudStorageTest, DamagedDataThrowsCloudStorageException)
{
    [[maybe_unused]] const auto written = storage()->saveDocument(largeDocument(), TimePoint{});
    m_server.stored->content["data"] = "AAAA";

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage()->loadDocument(), CloudStorageException);
}

TEST_F(CompressedCloudStorageTest, InvalidBase64ThrowsCloudStorageException)
{
    [[maybe_unused]] const auto written = storage()->saveDocument(largeDocument(), TimePoint{});
    m_server.stored->content["data"] = "not base64!";

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage()->loadDocument(), CloudStorageException);
}

TEST_F(CompressedCloudStorageTest, UnknownVersionThrowsCloudStorageException)
{
    [[maybe_unused]] const auto written = storage()->saveDocument(largeDocument(), TimePoint{});
    m_server.stored->content["version"] = 2;

    EXPECT_THROW([[maybe_unused]] const auto loaded = storage()->loadDocument(), CloudStorageException);
}

TEST_F(CompressedCloudStorageTest, CompressionHappensBeforeEncryption)
{
    const auto cloud = std::make_unique<CompressedCloudStorage>(
        std::make_unique<EncryptedCloudStorage>(std::make_unique<ForwardingCloudStorage>(m_server), "passphrase", "budget"),
        THRESHOLD);
    const auto document = largeDocument();

    [[maybe_unused]] const auto written = cloud->saveDocument(document, TimePoint{});
    const auto loaded = cloud->loadDocument();

    EXPECT_LT(m_server.stored->content.dump().size() * 2, document.dump().size());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, document);
}

TEST_F(CompressedCloudStorageTest, InvalidUtf8IsReplacedInsteadOfFailingTheSave)
{
    auto document = largeDocument();
    document["title"] = std::string("bad\xff title");
    const auto cloud = storage();

    [[maybe_unused]] const auto result = cloud->saveDocument(document, TimePoint{});
    const auto loaded = cloud->loadDocument();

    ASSERT_TRUE(m_server.stored.has_value());
    EXPECT_TRUE(CompressedCloudStorage::isCompressed(m_server.stored->content));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content.at("title"), "bad\xEF\xBF\xBD title");
}

TEST_F(CompressedCloudStorageTest, EncryptedAndCompressedDocumentIsBase64EncodedOnlyOnce)
{
    const auto cloud = std::make_unique<CompressedCloudStorage>(
        std::make_unique<EncryptedCloudStorage>(std::make_unique<ForwardingCloudStorage>(m_server), "passphrase", "budget"),
        THRESHOLD);
    const auto document = largeDocument();
    const auto gzipSize = stapik::compression::gzipCompress(stapik::text::dumpJson(document)).value().size();

    [[maybe_unused]] const auto written = cloud->saveDocument(document, TimePoint{});

    const auto ciphertextSize = m_server.stored->content.at("ciphertext").get<std::string>().size();
    EXPECT_LT(ciphertextSize, gzipSize * 4 / 3 + 64);
    EXPECT_EQ(cloud->loadDocument()->content, document);
}
