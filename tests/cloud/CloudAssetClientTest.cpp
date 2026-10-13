#include "stapik/cloud/CloudAssetClient.hpp"

#include "support/FakeHttpClient.hpp"
#include "support/ThrownMessage.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <tuple>
#include <vector>
#include <variant>

namespace
{
    using namespace std::chrono_literals;
    using namespace stapik::cloud;
    using stapik::http::Method;
    using stapik::test::FakeHttpClient;
    using stapik::test::thrownMessage;

    const CloudStorageConfig CONFIG{ .apiUrl = "https://cloud.example", .apiKey = "secret-key" };
    const std::string ASSETS_URL = "https://cloud.example/api/v1/assets/covers";
    const std::string ASSET_METADATA = R"({"filename":"a.jpg","mimeType":"image/jpeg","sizeBytes":3,"checksumSha256":"abc","updatedAt":"2026-01-15T10:30:00Z"})";
    const std::chrono::system_clock::time_point SERVER_TIME = std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::January / 15 } + 10h + 30min;
    const AssetBytes BINARY_CONTENT{ 0x00, 0xFF, 0x10, 0x80 };

    class CloudAssetClientTest : public testing::Test
    {
    protected:
        FakeHttpClient m_http;
        CloudAssetClient m_client{ CONFIG, "covers", m_http };
    };
}

TEST_F(CloudAssetClientTest, UnconfiguredClientNeverSendsARequest)
{
    const CloudAssetClient unconfigured(CloudStorageConfig{ .apiUrl = "https://cloud.example", .apiKey = {} }, "covers", m_http);

    EXPECT_TRUE(unconfigured.listAssets().empty());
    EXPECT_FALSE(unconfigured.downloadAsset("a.jpg").has_value());
    EXPECT_FALSE(unconfigured.deleteAsset("a.jpg"));
    EXPECT_THROW(std::ignore = unconfigured.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudStorageException);
    EXPECT_TRUE(m_http.requests.empty());
}

TEST_F(CloudAssetClientTest, ListSendsAuthenticatedGetToTheAssetsSlot)
{
    m_http.respondWith(200, R"({"assets":[]})");

    std::ignore = m_client.listAssets();

    ASSERT_EQ(m_http.requests.size(), 1U);
    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.method, Method::Get);
    EXPECT_EQ(request.url, ASSETS_URL);
    EXPECT_EQ(request.headers.at("x-api-key"), "secret-key");
    EXPECT_TRUE(std::holds_alternative<stapik::http::NoBody>(request.body));
}

TEST_F(CloudAssetClientTest, TransfersHaveNoOverallTimeButStillStallWhenIdle)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.timeout, stapik::http::NO_TIMEOUT);
    EXPECT_GT(request.stallTimeout, 0s);
}

TEST_F(CloudAssetClientTest, EveryRequestHasAResponseSizeLimit)
{
    m_http.respondWith(404);

    std::ignore = m_client.listAssets();

    EXPECT_TRUE(m_http.requests.front().maxResponseBytes.has_value());
}

TEST_F(CloudAssetClientTest, ListIsParsedFromTheResponse)
{
    m_http.respondWith(200, R"({"assets":[)" + ASSET_METADATA + "]}");

    const auto assets = m_client.listAssets();

    ASSERT_EQ(assets.size(), 1U);
    EXPECT_EQ(assets.front().filename, "a.jpg");
    EXPECT_EQ(assets.front().mimeType, "image/jpeg");
    EXPECT_EQ(assets.front().sizeBytes, 3);
    EXPECT_EQ(assets.front().checksumSha256, "abc");
    EXPECT_EQ(assets.front().updatedAt, SERVER_TIME);
}

TEST_F(CloudAssetClientTest, MissingSlotListsAsEmpty)
{
    m_http.respondWith(404);

    EXPECT_TRUE(m_client.listAssets().empty());
}

TEST_F(CloudAssetClientTest, UnexpectedStatusOnListIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.listAssets(); }), "API asset list failed with status 500");
}

TEST_F(CloudAssetClientTest, TransportFailureOnListIsReportedWithTheReason)
{
    m_http.failWith("Timeout was reached");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.listAssets(); }), "API asset list failed: Timeout was reached");
}

TEST_F(CloudAssetClientTest, DownloadReturnsTheBodyBytesUnchanged)
{
    m_http.respondWith(200, BINARY_CONTENT);

    const auto content = m_client.downloadAsset("a b.jpg");

    EXPECT_EQ(m_http.requests.front().method, Method::Get);
    EXPECT_EQ(m_http.requests.front().url, ASSETS_URL + "/a%20b.jpg");
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, BINARY_CONTENT);
}

TEST_F(CloudAssetClientTest, MissingFileDownloadsAsNullopt)
{
    m_http.respondWith(404);

    EXPECT_FALSE(m_client.downloadAsset("a.jpg").has_value());
}

TEST_F(CloudAssetClientTest, UnexpectedStatusOnDownloadIsReported)
{
    m_http.respondWith(403);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.downloadAsset("a.jpg"); }), "API asset download failed with status 403");
}

TEST_F(CloudAssetClientTest, TransportFailureOnDownloadIsReportedWithTheReason)
{
    m_http.failWith("Connection refused");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.downloadAsset("a.jpg"); }), "API asset download failed: Connection refused");
}

TEST_F(CloudAssetClientTest, UploadSendsTheFileAsMultipartPut)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.method, Method::Put);
    EXPECT_EQ(request.url, ASSETS_URL + "/a.jpg");
    EXPECT_EQ(request.headers.at("x-api-key"), "secret-key");

    const auto* file = std::get_if<stapik::http::MultipartFile>(&request.body);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->fieldName, "file");
    EXPECT_EQ(file->filename, "a.jpg");
    EXPECT_EQ(file->mimeType, "image/jpeg");
    EXPECT_EQ(file->content, BINARY_CONTENT);
}

TEST_F(CloudAssetClientTest, UploadWithoutHooksInstallsNoCallbacks)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_FALSE(m_http.requests.front().onProgress);
    EXPECT_FALSE(m_http.requests.front().isCancelled);
}

TEST_F(CloudAssetClientTest, UploadProgressIsLimitedToTheSizeOfTheFile)
{
    m_http.progressReports = { { .uploadedBytes = 2, .uploadTotalBytes = 300 }, { .uploadedBytes = 300, .uploadTotalBytes = 300 } };
    m_http.respondWith(201, ASSET_METADATA);
    std::vector<AssetUploadProgress> reported;

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT, { .onProgress = [&](const AssetUploadProgress& progress) { reported.push_back(progress); } });

    ASSERT_EQ(reported.size(), 2U);
    EXPECT_EQ(reported[0].sentBytes, 2U);
    EXPECT_EQ(reported[1].sentBytes, BINARY_CONTENT.size());
    EXPECT_EQ(reported[1].totalBytes, BINARY_CONTENT.size());
}

TEST_F(CloudAssetClientTest, CancelledUploadIsReportedAsCancelled)
{
    m_http.progressReports = { { .uploadedBytes = 1, .uploadTotalBytes = 4 } };
    m_http.respondWith(201, ASSET_METADATA);

    EXPECT_THROW(std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT, { .isCancelled = [] { return true; } }), AssetTransferCancelledException);
}

TEST_F(CloudAssetClientTest, UploadThatIsNotCancelledCompletes)
{
    m_http.progressReports = { { .uploadedBytes = 1, .uploadTotalBytes = 4 } };
    m_http.respondWith(201, ASSET_METADATA);

    const auto info = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT, { .isCancelled = [] { return false; } });

    EXPECT_EQ(info.filename, "a.jpg");
}

TEST_F(CloudAssetClientTest, UploadLeavesTheContentTypeToTheMultipartEncoder)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_FALSE(m_http.requests.front().headers.contains("Content-Type"));
}

TEST_F(CloudAssetClientTest, CreatedAndOverwrittenFilesReturnTheirMetadata)
{
    m_http.respondWith(201, ASSET_METADATA);
    m_http.respondWith(200, ASSET_METADATA);

    const auto created = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);
    const auto overwritten = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(created.filename, "a.jpg");
    EXPECT_EQ(created.updatedAt, SERVER_TIME);
    EXPECT_EQ(overwritten.checksumSha256, "abc");
}

TEST_F(CloudAssetClientTest, TooLargeFileIsReportedAsSuch)
{
    m_http.respondWith(413);

    EXPECT_THROW(std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), AssetTooLargeException);
}

TEST_F(CloudAssetClientTest, UnsupportedTypeIsReportedWithTheServerReason)
{
    m_http.respondWith(415, R"({"detail":"Detected content type image/gif is not allowed in this slot"})");

    EXPECT_EQ(thrownMessage<AssetTypeNotAllowedException>([&] { std::ignore = m_client.uploadAsset("a.gif", "image/gif", BINARY_CONTENT); }),
              "Detected content type image/gif is not allowed in this slot");
}

TEST_F(CloudAssetClientTest, UnsupportedTypeWithoutDetailGetsAGenericReason)
{
    m_http.respondWith(415);

    EXPECT_EQ(thrownMessage<AssetTypeNotAllowedException>([&] { std::ignore = m_client.uploadAsset("a.gif", "image/gif", BINARY_CONTENT); }),
              "File 'a.gif' has a type the cloud slot does not accept");
}

TEST_F(CloudAssetClientTest, UploadSendsTheNormalizedFilename)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a\xCC\xA8.jpg", "image/jpeg", BINARY_CONTENT);

    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.url, ASSETS_URL + "/%C4%85.jpg");
    EXPECT_EQ(std::get<stapik::http::MultipartFile>(request.body).filename, "\xC4\x85.jpg");
}

TEST_F(CloudAssetClientTest, InvalidFilenameIsRejectedWithoutARequest)
{
    EXPECT_THROW(std::ignore = m_client.uploadAsset("a:b.jpg", "image/jpeg", BINARY_CONTENT), InvalidAssetFilenameException);
    EXPECT_THROW(std::ignore = m_client.downloadAsset("a:b.jpg"), InvalidAssetFilenameException);
    EXPECT_THROW(std::ignore = m_client.deleteAsset("a:b.jpg"), InvalidAssetFilenameException);
    EXPECT_TRUE(m_http.requests.empty());
}

TEST_F(CloudAssetClientTest, UnexpectedStatusOnUploadIsReported)
{
    m_http.respondWith(401);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }), "API asset upload failed with status 401");
}

TEST_F(CloudAssetClientTest, TransportFailureOnUploadIsReportedWithTheReason)
{
    m_http.failWith("Could not resolve host");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }), "API asset upload failed: Could not resolve host");
}

TEST_F(CloudAssetClientTest, DeletedFileIsReportedAsDeleted)
{
    m_http.respondWith(204);

    EXPECT_TRUE(m_client.deleteAsset("a.jpg"));
    EXPECT_EQ(m_http.requests.front().method, Method::Delete);
    EXPECT_EQ(m_http.requests.front().url, ASSETS_URL + "/a.jpg");
}

TEST_F(CloudAssetClientTest, MissingFileIsNotDeleted)
{
    m_http.respondWith(404);

    EXPECT_FALSE(m_client.deleteAsset("a.jpg"));
}

TEST_F(CloudAssetClientTest, UnexpectedStatusOnDeleteIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.deleteAsset("a.jpg"); }), "API asset delete failed with status 500");
}

TEST_F(CloudAssetClientTest, TransportFailureOnDeleteIsReportedWithTheReason)
{
    m_http.failWith("Connection reset");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.deleteAsset("a.jpg"); }), "API asset delete failed: Connection reset");
}
