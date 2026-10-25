#include "stapik/cloud/CloudAssetClient.hpp"
#include "stapik/cloud/CloudAssetProtocol.hpp"
#include "stapik/task/Cancellation.hpp"

#include "support/FakeHttpClient.hpp"
#include "support/ThrownMessage.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
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
    const std::string UPLOAD_ID = "0b6f6a5e-1c3d-4f7a-9d2e-3a4b5c6d7e8f";
    const std::string UPLOADS_URL = "https://cloud.example/api/v1/asset-uploads";

    std::string assetListWithChecksum(const std::string& checksum)
    {
        nlohmann::json asset = nlohmann::json::parse(ASSET_METADATA);
        asset["sizeBytes"] = 4;
        asset["checksumSha256"] = checksum;
        return nlohmann::json{ { "assets", nlohmann::json::array({ asset }) } }.dump();
    }

    std::string uploadState(const int offset, const bool completed = false)
    {
        nlohmann::json state = { { "uploadId", UPLOAD_ID }, { "filename", "a.jpg" }, { "sizeBytes", 4 }, { "offset", offset }, { "expiresAt", "2026-01-16T10:30:00Z" } };
        if (completed)
            state["asset"] = nlohmann::json::parse(ASSET_METADATA);

        return state.dump();
    }

    class CloudAssetClientTest : public testing::Test
    {
    protected:
        FakeHttpClient m_http;
        CloudAssetClient m_client{ CONFIG, "covers", m_http };
        CloudAssetClient m_chunkedClient{ CONFIG, "covers", m_http, AssetChunking{ .thresholdBytes = 4, .chunkBytes = 3 } };
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

TEST_F(CloudAssetClientTest, ResponseSizeLimitIs64MiBByDefault)
{
    m_http.respondWith(404);

    std::ignore = m_client.listAssets();

    EXPECT_EQ(m_http.requests.front().maxResponseBytes, std::optional<std::size_t>{ 64U * 1024U * 1024U });
}

TEST_F(CloudAssetClientTest, ResponseSizeLimitGrowsToTheSlotLimit)
{
    const CloudAssetClient client(CONFIG, "covers", m_http, AssetChunking{}, nullptr, AssetSizeLimit{ .maxBytes = 200LL * 1024 * 1024 });
    m_http.respondWith(404);

    std::ignore = client.downloadAsset("a.jpg");

    EXPECT_EQ(m_http.requests.front().maxResponseBytes, std::optional<std::size_t>{ 200U * 1024U * 1024U });
}

TEST_F(CloudAssetClientTest, SmallerSlotLimitDoesNotShrinkTheResponseSizeLimit)
{
    const CloudAssetClient client(CONFIG, "covers", m_http, AssetChunking{}, nullptr, AssetSizeLimit{ .maxBytes = 10LL * 1024 * 1024 });
    m_http.respondWith(404);

    std::ignore = client.downloadAsset("a.jpg");

    EXPECT_EQ(m_http.requests.front().maxResponseBytes, std::optional<std::size_t>{ 64U * 1024U * 1024U });
}

TEST_F(CloudAssetClientTest, UnknownSlotLimitKeepsTheDefaultResponseSizeLimit)
{
    const CloudAssetClient client(CONFIG, "covers", m_http, AssetChunking{}, nullptr, AssetSizeLimit{});
    m_http.respondWith(404);

    std::ignore = client.downloadAsset("a.jpg");

    EXPECT_EQ(m_http.requests.front().maxResponseBytes, std::optional<std::size_t>{ 64U * 1024U * 1024U });
}

TEST_F(CloudAssetClientTest, DownloadAboveTheResponseSizeLimitIsReportedWithTheLimit)
{
    m_http.failWith("The response is larger than the limit of 67108864 bytes");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.downloadAsset("a.jpg"); }),
              "API asset download failed: The response is larger than the limit of 67108864 bytes");
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
    EXPECT_TRUE(std::ranges::equal(file->content, BINARY_CONTENT));
}

TEST_F(CloudAssetClientTest, UploadWithoutHooksInstallsNoProgressCallbackAndCancelsOnlyWithItsTask)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    const auto& request = m_http.requests.front();
    EXPECT_FALSE(request.onProgress);
    ASSERT_TRUE(request.isCancelled);
    EXPECT_FALSE(request.isCancelled());
}

TEST_F(CloudAssetClientTest, UploadIsCancelledWhenTheRunningTaskIsCancelled)
{
    m_http.progressReports.push_back({});
    std::stop_source source;
    source.request_stop();
    const stapik::task::CancellationScope scope(source.get_token());

    EXPECT_THROW(std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), AssetTransferCancelledException);
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

TEST_F(CloudAssetClientTest, UploadSendsTheChecksumOfTheContent)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(m_http.requests.front().headers.at("X-Checksum-Sha256"), sha256Hex(BINARY_CONTENT));
    EXPECT_EQ(m_http.requests.front().headers.at("x-api-key"), "secret-key");
}

TEST_F(CloudAssetClientTest, ChecksumMismatchIsReportedWithTheServerReason)
{
    m_http.respondWith(422, R"({"detail":"Uploaded content checksum aa does not match the expected bb"})");

    EXPECT_EQ(thrownMessage<AssetChecksumMismatchException>([&] { std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "Uploaded content checksum aa does not match the expected bb");
}

TEST_F(CloudAssetClientTest, ChecksumMismatchWithoutDetailGetsAGenericReason)
{
    m_http.respondWith(422);

    EXPECT_EQ(thrownMessage<AssetChecksumMismatchException>([&] { std::ignore = m_client.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "File 'a.jpg' was damaged on the way to the cloud");
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

TEST_F(CloudAssetClientTest, FileBelowTheThresholdIsSentInOneRequest)
{
    m_http.respondWith(201, ASSET_METADATA);

    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", AssetBytes{ 0x01, 0x02, 0x03 });

    ASSERT_EQ(m_http.requests.size(), 1U);
    EXPECT_EQ(m_http.requests.front().method, Method::Put);
}

TEST_F(CloudAssetClientTest, LargeFileIsUploadedInChunks)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));

    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.filename, "a.jpg");
    ASSERT_EQ(m_http.requests.size(), 3U);

    const auto& start = m_http.requests[0];
    EXPECT_EQ(start.method, Method::Post);
    EXPECT_EQ(start.url, UPLOADS_URL);
    EXPECT_EQ(start.headers.at("x-api-key"), "secret-key");
    EXPECT_EQ(start.headers.at("Content-Type"), "application/json");
    const auto description = nlohmann::json::parse(stapik::test::bodyText(start));
    EXPECT_EQ(description.at("slotKey"), "covers");
    EXPECT_EQ(description.at("filename"), "a.jpg");
    EXPECT_EQ(description.at("sizeBytes"), 4);
    EXPECT_EQ(description.at("mimeType"), "image/jpeg");
    EXPECT_EQ(description.at("checksumSha256"), sha256Hex(BINARY_CONTENT));

    const auto& first = m_http.requests[1];
    EXPECT_EQ(first.method, Method::Patch);
    EXPECT_EQ(first.url, UPLOADS_URL + "/" + UPLOAD_ID);
    EXPECT_EQ(first.headers.at("Content-Type"), "application/octet-stream");
    EXPECT_EQ(first.headers.at("Upload-Offset"), "0");
    EXPECT_EQ(std::get<stapik::http::Bytes>(first.body), (stapik::http::Bytes{ 0x00, 0xFF, 0x10 }));

    const auto& last = m_http.requests[2];
    EXPECT_EQ(last.headers.at("Upload-Offset"), "3");
    EXPECT_EQ(std::get<stapik::http::Bytes>(last.body), (stapik::http::Bytes{ 0x80 }));
}

TEST_F(CloudAssetClientTest, ChunkedUploadReportsTheProgressOfTheWholeFile)
{
    m_http.progressReports = { { .uploadedBytes = 2, .uploadTotalBytes = 3 } };
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));

    std::vector<AssetUploadProgress> reports;
    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT, { .onProgress = [&reports](const AssetUploadProgress& progress) { reports.push_back(progress); } });

    ASSERT_EQ(reports.size(), 2U);
    EXPECT_EQ(reports[0].sentBytes, 2U);
    EXPECT_EQ(reports[1].sentBytes, 4U);
    EXPECT_EQ(reports[0].totalBytes, 4U);
    EXPECT_EQ(reports[1].totalBytes, 4U);
}

TEST_F(CloudAssetClientTest, ChunkedUploadCanBeCancelledBetweenChunks)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(204);

    int checks = 0;
    const auto cancelOnSecondCheck = [&checks] { return ++checks > 1; };

    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT, { .isCancelled = cancelOnSecondCheck }), AssetTransferCancelledException);
    ASSERT_EQ(m_http.requests.size(), 3U);
    EXPECT_EQ(m_http.requests.back().method, Method::Delete);
    EXPECT_EQ(m_http.requests.back().url, UPLOADS_URL + "/" + UPLOAD_ID);
}

TEST_F(CloudAssetClientTest, InterruptedChunkIsSentAgainAfterAskingTheCloud)
{
    m_http.respondWith(201, uploadState(0));
    m_http.failWith("Connection reset");
    m_http.respondWith(200, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));

    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.filename, "a.jpg");
    ASSERT_EQ(m_http.requests.size(), 5U);
    EXPECT_EQ(m_http.requests[2].method, Method::Get);
    EXPECT_EQ(m_http.requests[2].url, UPLOADS_URL + "/" + UPLOAD_ID);
    EXPECT_EQ(m_http.requests[3].headers.at("Upload-Offset"), "0");
    EXPECT_EQ(m_http.requests[4].headers.at("Upload-Offset"), "3");
}

TEST_F(CloudAssetClientTest, RateLimitedChunkIsNotTreatedAsAnInterruption)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWithRetryAfter(429, 90s);

    try
    {
        std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);
        FAIL() << "Expected CloudRateLimitedException";
    }
    catch (const CloudRateLimitedException& exception)
    {
        EXPECT_EQ(exception.retryAfter(), std::optional{ 90s });
    }

    EXPECT_EQ(m_http.requests.front().method, Method::Post);
    EXPECT_EQ(m_http.requests[1].method, Method::Patch);
    EXPECT_EQ(m_http.requests.size(), 2U);
}

TEST_F(CloudAssetClientTest, RateLimitedUploadContinuesFromTheOffsetTheCloudReports)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWithRetryAfter(429, 90s);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudRateLimitedException);

    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));
    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.filename, "a.jpg");
    ASSERT_EQ(m_http.requests.size(), 5U);
    EXPECT_EQ(m_http.requests[3].method, Method::Get);
    EXPECT_EQ(m_http.requests[3].url, UPLOADS_URL + "/" + UPLOAD_ID);
    EXPECT_EQ(m_http.requests[4].method, Method::Patch);
    EXPECT_EQ(m_http.requests[4].headers.at("Upload-Offset"), "3");
    EXPECT_EQ(std::get<stapik::http::Bytes>(m_http.requests[4].body), (stapik::http::Bytes{ 0x80 }));
}

TEST_F(CloudAssetClientTest, RateLimitedUploadStartsAgainWhenTheCloudForgotIt)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWithRetryAfter(429, 90s);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudRateLimitedException);

    m_http.respondWith(404);
    m_http.respondWith(200, R"({"assets":[]})");
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));
    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.filename, "a.jpg");
    ASSERT_EQ(m_http.requests.size(), 7U);
    EXPECT_EQ(m_http.requests[4].method, Method::Post);
    EXPECT_EQ(m_http.requests[5].headers.at("Upload-Offset"), "0");
    EXPECT_EQ(m_http.requests[6].headers.at("Upload-Offset"), "3");
}

TEST_F(CloudAssetClientTest, RateLimitedUploadIsNotSentAgainWhenTheCloudAlreadyStoredTheFile)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWithRetryAfter(429, 90s);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudRateLimitedException);

    m_http.respondWith(404);
    m_http.respondWith(200, assetListWithChecksum(sha256Hex(BINARY_CONTENT)));
    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.filename, "a.jpg");
    EXPECT_EQ(m_http.requests.size(), 4U);
}

TEST_F(CloudAssetClientTest, ChangedFileDoesNotContinueAnEarlierRateLimitedUpload)
{
    const AssetBytes changedContent{ 0x01, 0x02, 0x03, 0x04 };
    m_http.respondWith(201, uploadState(0));
    m_http.respondWithRetryAfter(429, 90s);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudRateLimitedException);

    m_http.respondWith(204);
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));
    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", changedContent);

    ASSERT_EQ(m_http.requests.size(), 6U);
    EXPECT_EQ(m_http.requests[2].method, Method::Delete);
    EXPECT_EQ(m_http.requests[2].url, UPLOADS_URL + "/" + UPLOAD_ID);
    EXPECT_EQ(m_http.requests[3].method, Method::Post);
    EXPECT_EQ(m_http.requests[4].headers.at("Upload-Offset"), "0");
}

TEST_F(CloudAssetClientTest, FailedUploadAfterARateLimitedOneIsAbortedAndForgotten)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWithRetryAfter(429, 90s);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudRateLimitedException);

    m_http.respondWith(200, uploadState(0));
    m_http.respondWith(403);
    m_http.respondWith(204);
    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), CloudStorageException);
    EXPECT_EQ(m_http.requests.back().method, Method::Delete);

    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));
    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(m_http.requests[m_http.requests.size() - 3].method, Method::Post);
}

TEST_F(CloudAssetClientTest, ExpiryOfTheApiKeyIsPassedToTheMonitor)
{
    const auto monitor = std::make_shared<ApiKeyExpiryMonitor>();
    const CloudAssetClient client(CONFIG, "covers", m_http, AssetChunking{}, monitor);
    m_http.respondWithHeaders(200, R"({"assets":[]})", { { "x-api-key-expires-at", "2026-11-01T08:00:00Z" } });

    EXPECT_TRUE(client.listAssets().empty());

    ASSERT_TRUE(monitor->expiresAt().has_value());
    EXPECT_EQ(*monitor->expiresAt(), std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::November / 1 } + 8h);
}

TEST_F(CloudAssetClientTest, UploadContinuesFromTheOffsetTheCloudReports)
{
    m_http.respondWith(201, uploadState(0));
    m_http.failWith("Connection reset");
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));

    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    ASSERT_EQ(m_http.requests.size(), 4U);
    EXPECT_EQ(m_http.requests[3].headers.at("Upload-Offset"), "3");
    EXPECT_EQ(std::get<stapik::http::Bytes>(m_http.requests[3].body), (stapik::http::Bytes{ 0x80 }));
}

TEST_F(CloudAssetClientTest, OffsetConflictIsResolvedByAskingTheCloud)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(409);
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4, true));

    std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    ASSERT_EQ(m_http.requests.size(), 4U);
    EXPECT_EQ(m_http.requests[3].headers.at("Upload-Offset"), "3");
}

TEST_F(CloudAssetClientTest, LostAnswerToTheLastChunkIsRecoveredFromTheAssetList)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.failWith("Connection reset");
    m_http.respondWith(404);
    m_http.respondWith(200, assetListWithChecksum(sha256Hex(BINARY_CONTENT)));

    const auto info = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT);

    EXPECT_EQ(info.checksumSha256, sha256Hex(BINARY_CONTENT));
    ASSERT_EQ(m_http.requests.size(), 5U);
    EXPECT_EQ(m_http.requests.back().method, Method::Get);
    EXPECT_EQ(m_http.requests.back().url, ASSETS_URL);
}

TEST_F(CloudAssetClientTest, UploadLostByTheCloudIsReportedAndAborted)
{
    m_http.respondWith(201, uploadState(0));
    m_http.failWith("Connection reset");
    m_http.respondWith(404);
    m_http.respondWith(200, R"({"assets":[]})");
    m_http.respondWith(404);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "The cloud lost the upload of 'a.jpg'");
    ASSERT_EQ(m_http.requests.size(), 5U);
    EXPECT_EQ(m_http.requests.back().method, Method::Delete);
}

TEST_F(CloudAssetClientTest, UploadGivesUpAfterRepeatedInterruptions)
{
    m_http.respondWith(201, uploadState(0));
    for (int interruption = 0; interruption < 3; ++interruption)
    {
        m_http.failWith("Connection reset");
        m_http.respondWith(200, uploadState(0));
    }
    m_http.failWith("Connection reset");
    m_http.respondWith(204);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "API asset upload failed: Connection reset");
    ASSERT_EQ(m_http.requests.size(), 9U);
    EXPECT_EQ(m_http.requests.back().method, Method::Delete);
}

TEST_F(CloudAssetClientTest, RefusedUploadIsAbortedInTheCloud)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(415, R"({"detail":"Detected content type image/gif is not allowed in this slot"})");
    m_http.respondWith(204);

    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.gif", "image/gif", BINARY_CONTENT), AssetTypeNotAllowedException);
    ASSERT_EQ(m_http.requests.size(), 3U);
    EXPECT_EQ(m_http.requests.back().method, Method::Delete);
    EXPECT_EQ(m_http.requests.back().url, UPLOADS_URL + "/" + UPLOAD_ID);
}

TEST_F(CloudAssetClientTest, TooLargeFileIsRefusedWhenTheUploadStarts)
{
    m_http.respondWith(413);

    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), AssetTooLargeException);
    EXPECT_EQ(m_http.requests.size(), 1U);
}

TEST_F(CloudAssetClientTest, UnexpectedStatusWhenTheUploadStartsIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }), "API asset upload failed with status 500");
}

TEST_F(CloudAssetClientTest, ChunkRefusedAsTooBigIsNotReportedAsATooLargeFile)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(413);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "API asset upload failed: the server accepts chunks smaller than 3 bytes");
}

TEST_F(CloudAssetClientTest, UnsupportedTypeIsReportedWhenTheFirstChunkArrives)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(415, R"({"detail":"Detected content type image/gif is not allowed in this slot"})");

    EXPECT_EQ(thrownMessage<AssetTypeNotAllowedException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.gif", "image/gif", BINARY_CONTENT); }),
              "Detected content type image/gif is not allowed in this slot");
}

TEST_F(CloudAssetClientTest, ChecksumMismatchIsReportedWhenTheLastChunkArrives)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(422);

    EXPECT_THROW(std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT), AssetChecksumMismatchException);
}

TEST_F(CloudAssetClientTest, UnexpectedAcknowledgedOffsetIsReported)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(2));

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "The cloud acknowledged an unexpected part of the upload");
}

TEST_F(CloudAssetClientTest, UploadWithoutTheStoredAssetIsNotReportedAsDone)
{
    m_http.respondWith(201, uploadState(0));
    m_http.respondWith(200, uploadState(3));
    m_http.respondWith(200, uploadState(4));

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_chunkedClient.uploadAsset("a.jpg", "image/jpeg", BINARY_CONTENT); }),
              "The cloud did not confirm that the upload is complete");
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
