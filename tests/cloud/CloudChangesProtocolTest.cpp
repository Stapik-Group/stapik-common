#include "stapik/cloud/CloudChangesProtocol.hpp"
#include "stapik/cloud/CloudStorageException.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <tuple>

namespace
{
    using namespace std::chrono_literals;
    using namespace stapik::cloud;

    const std::chrono::system_clock::time_point SINCE = std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::January / 15 } + 10h + 30min + 5s + 123456us;
}

TEST(CloudChangesProtocolTest, UrlCarriesTheMomentWithMicrosecondsAndEncoded)
{
    EXPECT_EQ(changesUrl("https://cloud.example", SINCE), "https://cloud.example/api/v1/changes?since=2026-01-15T10%3A30%3A05.123456Z");
}

TEST(CloudChangesProtocolTest, ChangesAreParsedInServerOrder)
{
    const std::string body = R"({"serverTime":"2026-01-15T10:31:00.5Z","changes":[
        {"slotKey":"budget","sizeBytes":10,"contentHash":"aaa","updatedAt":"2026-01-15T10:30:10Z","deleted":false},
        {"slotKey":"budget","partition":"2025","sizeBytes":0,"contentHash":"bbb","updatedAt":"2026-01-15T10:30:20Z","deleted":true}
    ]})";

    const auto list = parseChangeList(body);

    EXPECT_EQ(list.serverTime, std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::January / 15 } + 10h + 31min + 500ms);
    ASSERT_EQ(list.changes.size(), 2U);
    EXPECT_EQ(list.changes[0].slotKey, "budget");
    EXPECT_FALSE(list.changes[0].partition.has_value());
    EXPECT_EQ(list.changes[0].sizeBytes, 10);
    EXPECT_EQ(list.changes[0].contentHash, "aaa");
    EXPECT_FALSE(list.changes[0].deleted);
    EXPECT_EQ(list.changes[1].partition, std::optional<std::string>{ "2025" });
    EXPECT_TRUE(list.changes[1].deleted);
}

TEST(CloudChangesProtocolTest, EmptyFeedStillCarriesTheServerTime)
{
    const auto list = parseChangeList(R"({"serverTime":"2026-01-15T10:31:00Z","changes":[]})");

    EXPECT_TRUE(list.changes.empty());
    EXPECT_EQ(list.serverTime, std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::January / 15 } + 10h + 31min);
}

TEST(CloudChangesProtocolTest, MalformedBodyIsReportedAsAStorageError)
{
    EXPECT_THROW(std::ignore = parseChangeList("not json"), CloudStorageException);
    EXPECT_THROW(std::ignore = parseChangeList(R"({"changes":[]})"), CloudStorageException);
    EXPECT_THROW(std::ignore = parseChangeList(R"({"serverTime":"yesterday","changes":[]})"), CloudStorageException);
}
