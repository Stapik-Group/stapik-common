#include "stapik/cloud/CloudPartitionProtocol.hpp"
#include "stapik/cloud/CloudStorageException.hpp"

#include <gtest/gtest.h>

#include <string>
#include <tuple>

namespace
{
    using namespace stapik::cloud;

    const std::string DOCUMENT_URL = "https://cloud.example.com/api/v1/documents/budget";
}

TEST(CloudPartitionProtocolTest, PlainYearKeyIsLeftUntouched)
{
    EXPECT_EQ(encodePathSegment("2025"), "2025");
    EXPECT_EQ(encodePathSegment("archive-2025_v1.0~x"), "archive-2025_v1.0~x");
}

TEST(CloudPartitionProtocolTest, ReservedAndNonAsciiBytesArePercentEncoded)
{
    EXPECT_EQ(encodePathSegment("a/b"), "a%2Fb");
    EXPECT_EQ(encodePathSegment("a b?c"), "a%20b%3Fc");
    EXPECT_EQ(encodePathSegment("zażółć"), "za%C5%BC%C3%B3%C5%82%C4%87");
}

TEST(CloudPartitionProtocolTest, UrlsAreBuiltUnderTheDocumentSlot)
{
    EXPECT_EQ(partitionsUrl(DOCUMENT_URL), DOCUMENT_URL + "/partitions");
    EXPECT_EQ(partitionUrl(DOCUMENT_URL, "2025"), DOCUMENT_URL + "/partitions/2025");
    EXPECT_EQ(partitionUrl(DOCUMENT_URL, "../main"), DOCUMENT_URL + "/partitions/..%2Fmain");
}

TEST(CloudPartitionProtocolTest, ListIsParsedInServerOrder)
{
    const std::string body = R"({"partitions":[
        {"partition":"2024","sizeBytes":120,"contentHash":"abc","updatedAt":"2025-01-02T03:04:05Z"},
        {"partition":"2025","sizeBytes":4096,"contentHash":"def","updatedAt":"2026-01-02T03:04:05.123456Z"}
    ]})";

    const auto list = parsePartitionList(body);

    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].partition, "2024");
    EXPECT_EQ(list[0].sizeBytes, 120);
    EXPECT_EQ(list[0].contentHash, "abc");
    EXPECT_EQ(list[1].partition, "2025");
    EXPECT_EQ(list[1].sizeBytes, 4096);
    EXPECT_GT(list[1].updatedAt, list[0].updatedAt);
}

TEST(CloudPartitionProtocolTest, EmptyListIsValid)
{
    EXPECT_TRUE(parsePartitionList(R"({"partitions":[]})").empty());
}

TEST(CloudPartitionProtocolTest, MalformedBodyIsReportedAsCloudStorageException)
{
    EXPECT_THROW(std::ignore = parsePartitionList("not json"), CloudStorageException);
    EXPECT_THROW(std::ignore = parsePartitionList(R"({"other":[]})"), CloudStorageException);
    EXPECT_THROW(std::ignore = parsePartitionList(R"({"partitions":[{"partition":"2025"}]})"), CloudStorageException);
}

TEST(CloudPartitionProtocolTest, InvalidTimestampIsReportedAsCloudStorageException)
{
    const std::string body = R"({"partitions":[{"partition":"2025","sizeBytes":1,"contentHash":"x","updatedAt":"yesterday"}]})";

    EXPECT_THROW(std::ignore = parsePartitionList(body), CloudStorageException);
}
