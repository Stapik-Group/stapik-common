#include "stapik/cloud/CloudAssetProtocol.hpp"
#include "stapik/cloud/CloudStorageException.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <type_traits>

namespace
{
    using namespace stapik::cloud;

    const std::string API_URL = "https://cloud.example.com";

    std::int64_t epochSeconds(const std::chrono::system_clock::time_point& timePoint)
    {
        return std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
    }
}

TEST(CloudAssetProtocolTest, UrlsAreBuiltUnderTheAssetsSlot)
{
    EXPECT_EQ(assetsUrl(API_URL, "covers"), API_URL + "/api/v1/assets/covers");
    EXPECT_EQ(assetUrl(API_URL, "covers", "abc123.jpg"), API_URL + "/api/v1/assets/covers/abc123.jpg");
}

TEST(CloudAssetProtocolTest, SlotKeyAndFilenameArePercentEncoded)
{
    EXPECT_EQ(assetUrl(API_URL, "my covers", "a/b.jpg"), API_URL + "/api/v1/assets/my%20covers/a%2Fb.jpg");
    EXPECT_EQ(assetUrl(API_URL, "covers", "../main"), API_URL + "/api/v1/assets/covers/..%2Fmain");
}

TEST(CloudAssetProtocolTest, ListIsParsedInServerOrder)
{
    const std::string body = R"({"assets":[
        {"filename":"a.jpg","mimeType":"image/jpeg","sizeBytes":120,"checksumSha256":"abc","updatedAt":"2025-01-02T03:04:05Z"},
        {"filename":"b.png","mimeType":"image/png","sizeBytes":4096,"checksumSha256":"def","updatedAt":"2026-01-02T03:04:05.123456Z"}
    ]})";

    const auto assets = parseAssetList(body);

    ASSERT_EQ(assets.size(), 2U);
    EXPECT_EQ(assets[0].filename, "a.jpg");
    EXPECT_EQ(assets[0].mimeType, "image/jpeg");
    EXPECT_EQ(assets[0].sizeBytes, 120);
    EXPECT_EQ(assets[0].checksumSha256, "abc");
    EXPECT_EQ(epochSeconds(assets[0].updatedAt), 1735787045);
    EXPECT_EQ(assets[1].filename, "b.png");
    EXPECT_EQ(epochSeconds(assets[1].updatedAt), 1767323045);
}

TEST(CloudAssetProtocolTest, EmptyOrMissingListIsEmpty)
{
    EXPECT_TRUE(parseAssetList(R"({"assets":[]})").empty());
    EXPECT_TRUE(parseAssetList("{}").empty());
}

TEST(CloudAssetProtocolTest, MalformedListIsReportedAsCloudError)
{
    EXPECT_THROW(static_cast<void>(parseAssetList("not json")), CloudStorageException);
    EXPECT_THROW(static_cast<void>(parseAssetList(R"({"assets":[{"filename":"a.jpg"}]})")), CloudStorageException);
    EXPECT_THROW(
        static_cast<void>(parseAssetList(R"({"assets":[{"filename":"a","mimeType":"x","sizeBytes":1,"checksumSha256":"c","updatedAt":"yesterday"}]})")),
        CloudStorageException);
}

TEST(CloudAssetProtocolTest, MetadataOfASingleFileIsParsed)
{
    const auto asset = parseAssetMetadata(
        R"({"filename":"a.jpg","mimeType":"image/jpeg","sizeBytes":7,"checksumSha256":"abc","updatedAt":"2025-01-02T03:04:05Z"})");

    EXPECT_EQ(asset.filename, "a.jpg");
    EXPECT_EQ(asset.sizeBytes, 7);
    EXPECT_THROW(static_cast<void>(parseAssetMetadata("{}")), CloudStorageException);
}

TEST(CloudAssetProtocolTest, Sha256MatchesKnownVectors)
{
    EXPECT_EQ(sha256Hex({}), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    const AssetBytes abc = { 'a', 'b', 'c' };
    EXPECT_EQ(sha256Hex(abc), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(CloudAssetProtocolTest, TooLargeFileIsACloudStorageException)
{
    static_assert(std::is_base_of_v<CloudStorageException, AssetTooLargeException>);

    EXPECT_THROW(throw AssetTooLargeException("too large"), CloudStorageException);
}
