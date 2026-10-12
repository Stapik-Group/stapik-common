#include "stapik/cloud/AssetCache.hpp"

#include "support/FakeAssetStorage.hpp"

#include <gtest/gtest.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::cloud::AssetBytes;
    using stapik::cloud::AssetCache;
    using stapik::cloud::AssetCacheError;
    using stapik::cloud::AssetInfo;
    using stapik::cloud::sha256Hex;

    AssetBytes bytesOf(const std::string& text)
    {
        return { text.begin(), text.end() };
    }

    AssetInfo infoOf(const std::string& filename, const std::string& text)
    {
        return AssetInfo{ .filename = filename, .checksumSha256 = sha256Hex(bytesOf(text)) };
    }

    std::size_t countEntries(const fs::path& directory)
    {
        std::size_t count = 0;
        for (const auto& entry : fs::directory_iterator(directory))
        {
            static_cast<void>(entry);
            ++count;
        }
        return count;
    }

    class AssetCacheTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-assetcache-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(AssetCacheTest, FindsNothingInEmptyCache)
{
    const AssetCache cache(m_directory / "assets");

    EXPECT_FALSE(cache.find(infoOf("a.png", "content")));
}

TEST_F(AssetCacheTest, ReturnsStoredContent)
{
    const AssetCache cache(m_directory / "assets");
    const auto info = infoOf("a.png", "content");

    ASSERT_TRUE(cache.store(info, bytesOf("content")));

    EXPECT_EQ(cache.find(info), bytesOf("content"));
}

TEST_F(AssetCacheTest, StoresEmptyFile)
{
    const AssetCache cache(m_directory / "assets");
    const auto info = infoOf("empty.txt", "");

    ASSERT_TRUE(cache.store(info, {}));

    EXPECT_EQ(cache.find(info), AssetBytes{});
}

TEST_F(AssetCacheTest, RefusesContentWithWrongChecksum)
{
    const AssetCache cache(m_directory / "assets");

    const auto result = cache.store(infoOf("a.png", "expected"), bytesOf("other"));

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().reason, AssetCacheError::Reason::ChecksumMismatch);
    EXPECT_FALSE(cache.find(infoOf("a.png", "expected")));
}

TEST_F(AssetCacheTest, NeverCachesAssetWithoutValidChecksum)
{
    const AssetCache cache(m_directory / "assets");

    for (const std::string checksum : { "", "abc", "../../etc/passwd" })
    {
        const AssetInfo info{ .filename = "a.png", .checksumSha256 = checksum };
        const auto result = cache.store(info, bytesOf("content"));

        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().reason, AssetCacheError::Reason::InvalidChecksum);
        EXPECT_FALSE(cache.find(info));
    }
    EXPECT_FALSE(fs::exists(m_directory / "assets"));
}

TEST_F(AssetCacheTest, ReportsWriteFailure)
{
    std::ofstream(m_directory / "assets", std::ios::binary) << "a file where the directory should be";
    const AssetCache cache(m_directory / "assets");

    const auto result = cache.store(infoOf("a.png", "content"), bytesOf("content"));

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().reason, AssetCacheError::Reason::WriteFailed);
}

TEST_F(AssetCacheTest, AcceptsUppercaseChecksum)
{
    const AssetCache cache(m_directory / "assets");
    auto info = infoOf("a.png", "content");
    ASSERT_TRUE(cache.store(info, bytesOf("content")));

    for (auto& character : info.checksumSha256)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));

    EXPECT_EQ(cache.find(info), bytesOf("content"));
}

TEST_F(AssetCacheTest, DiscardsEntryThatChangedOnDisk)
{
    const AssetCache cache(m_directory / "assets");
    const auto info = infoOf("a.png", "content");
    ASSERT_TRUE(cache.store(info, bytesOf("content")));

    std::ofstream(m_directory / "assets" / info.checksumSha256, std::ios::binary) << "tampered";

    EXPECT_FALSE(cache.find(info));
    EXPECT_EQ(countEntries(m_directory / "assets"), 0U);
}

TEST_F(AssetCacheTest, FetchDownloadsAndCachesOnMiss)
{
    const AssetCache cache(m_directory / "assets");
    stapik::test::FakeAssetStorage storage;
    storage.put("a.png", "content");
    const auto info = infoOf("a.png", "content");

    EXPECT_EQ(cache.fetch(storage, info), bytesOf("content"));

    EXPECT_EQ(cache.find(info), bytesOf("content"));
}

TEST_F(AssetCacheTest, FetchUsesCacheWithoutTouchingStorage)
{
    const AssetCache cache(m_directory / "assets");
    stapik::test::FakeAssetStorage storage;
    const auto info = infoOf("a.png", "content");
    ASSERT_TRUE(cache.store(info, bytesOf("content")));
    storage.unreachable = true;

    EXPECT_EQ(cache.fetch(storage, info), bytesOf("content"));
}

TEST_F(AssetCacheTest, FetchDownloadsAgainWhenAssetChanged)
{
    const AssetCache cache(m_directory / "assets");
    stapik::test::FakeAssetStorage storage;
    storage.put("a.png", "old");
    ASSERT_TRUE(cache.fetch(storage, infoOf("a.png", "old")));

    storage.put("a.png", "new");

    EXPECT_EQ(cache.fetch(storage, infoOf("a.png", "new")), bytesOf("new"));
}

TEST_F(AssetCacheTest, FetchReturnsNothingForMissingFile)
{
    const AssetCache cache(m_directory / "assets");
    const stapik::test::FakeAssetStorage storage;

    EXPECT_FALSE(cache.fetch(storage, infoOf("a.png", "content")));
}

TEST_F(AssetCacheTest, FetchRejectsDownloadWithWrongChecksum)
{
    const AssetCache cache(m_directory / "assets");
    stapik::test::FakeAssetStorage storage;
    storage.put("a.png", "changed since listing");
    const auto info = infoOf("a.png", "content");

    EXPECT_THROW(static_cast<void>(cache.fetch(storage, info)), ::CloudStorageException);
    EXPECT_FALSE(fs::exists(m_directory / "assets"));
}

TEST_F(AssetCacheTest, FetchDoesNotCacheAssetWithoutChecksum)
{
    const AssetCache cache(m_directory / "assets");
    stapik::test::FakeAssetStorage storage;
    storage.put("a.png", "content");
    const AssetInfo info{ .filename = "a.png" };

    EXPECT_EQ(cache.fetch(storage, info), bytesOf("content"));
    EXPECT_FALSE(fs::exists(m_directory / "assets"));
}

TEST_F(AssetCacheTest, RetainOnlyRemovesUnlistedEntries)
{
    const AssetCache cache(m_directory / "assets");
    const auto kept = infoOf("kept.png", "kept");
    const auto dropped = infoOf("dropped.png", "dropped");
    ASSERT_TRUE(cache.store(kept, bytesOf("kept")));
    ASSERT_TRUE(cache.store(dropped, bytesOf("dropped")));

    cache.retainOnly({ kept });

    EXPECT_TRUE(cache.find(kept));
    EXPECT_FALSE(cache.find(dropped));
}

TEST_F(AssetCacheTest, RetainOnlyToleratesMissingDirectory)
{
    const AssetCache cache(m_directory / "missing");

    EXPECT_NO_THROW(cache.retainOnly({}));
}
