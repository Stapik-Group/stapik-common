#include "stapik/cloud/CloudAssetProtocol.hpp"
#include "stapik/cloud/CloudStorageException.hpp"

#include "support/ThrownMessage.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{
    using namespace stapik::cloud;
    using stapik::test::thrownMessage;

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

TEST(CloudAssetProtocolTest, FilenameRejectionsAreCloudStorageExceptions)
{
    static_assert(std::is_base_of_v<CloudStorageException, InvalidAssetFilenameException>);
    static_assert(std::is_base_of_v<CloudStorageException, AssetTypeNotAllowedException>);

    EXPECT_THROW(throw InvalidAssetFilenameException("bad name"), CloudStorageException);
    EXPECT_THROW(throw AssetTypeNotAllowedException("bad type"), CloudStorageException);
}

TEST(CloudAssetProtocolTest, PlainFilenameIsKept)
{
    EXPECT_EQ(normalizeAssetFilename("cover 1.jpg"), "cover 1.jpg");
    EXPECT_EQ(normalizeAssetFilename("COM0"), "COM0");
    EXPECT_EQ(normalizeAssetFilename("console.txt"), "console.txt");
}

TEST(CloudAssetProtocolTest, FilenameIsComposedToNfc)
{
    EXPECT_EQ(normalizeAssetFilename("a\xCC\xA8.jpg"), "\xC4\x85.jpg");
    EXPECT_EQ(normalizeAssetFilename("\xC4\x85.jpg"), "\xC4\x85.jpg");
}

TEST(CloudAssetProtocolTest, FilenamesTheServerRefusesAreRejected)
{
    const std::vector<std::string> refused = {
        "", "   ", "a/b", "a\\b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b",
        "name.", "name ", " name", "a\tb", "a\nb",
        "CON", "con", "PRN.txt", "Aux.tar.gz", "NUL .txt", "COM1", "lpt9.log",
        "a\xE2\x80\x8B" "b",
        "a\xEE\x80\x80" "b",
        "a\xFF" "b",
        std::string(256, 'a')
    };

    for (const auto& name : refused)
        EXPECT_THROW(std::ignore = normalizeAssetFilename(name), InvalidAssetFilenameException) << name;
}

TEST(CloudAssetProtocolTest, FilenameLengthIsCountedInUtf8Bytes)
{
    EXPECT_NO_THROW(std::ignore = normalizeAssetFilename(std::string(255, 'a')));

    std::string polish;
    for (int letter = 0; letter < 127; ++letter)
        polish += "\xC4\x85";

    EXPECT_NO_THROW(std::ignore = normalizeAssetFilename(polish));

    polish += "\xC4\x85";
    EXPECT_THROW(std::ignore = normalizeAssetFilename(polish), InvalidAssetFilenameException);
}

TEST(CloudAssetProtocolTest, RejectedFilenameExplainsWhy)
{
    EXPECT_EQ(thrownMessage<InvalidAssetFilenameException>([] { std::ignore = normalizeAssetFilename(" "); }), "Invalid filename: must not be blank");
    EXPECT_EQ(thrownMessage<InvalidAssetFilenameException>([] { std::ignore = normalizeAssetFilename("a:b"); }), "Invalid filename: contains forbidden character U+003A");
    EXPECT_EQ(thrownMessage<InvalidAssetFilenameException>([] { std::ignore = normalizeAssetFilename("con.txt"); }), "Invalid filename: is a reserved system name");
}

TEST(CloudAssetProtocolTest, ProblemDetailIsReadFromTheErrorBody)
{
    EXPECT_EQ(parseProblemDetail(R"({"title":"x","detail":"Why not"})"), "Why not");
    EXPECT_FALSE(parseProblemDetail(R"({"title":"x"})").has_value());
    EXPECT_FALSE(parseProblemDetail("not json").has_value());
    EXPECT_FALSE(parseProblemDetail("").has_value());
}
