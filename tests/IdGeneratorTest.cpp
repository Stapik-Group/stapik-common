#include "stapik/domain/IdGenerator.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace stapik::domain;

namespace
{
    bool isLowercaseHex(const std::string& text)
    {
        return std::ranges::all_of(text, [](const char character)
        {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        });
    }
}

TEST(IdGeneratorTest, FormatHexIdPadsToSixteenDigits)
{
    EXPECT_EQ(formatHexId(0), "0000000000000000");
    EXPECT_EQ(formatHexId(0xABCDEFULL), "0000000000abcdef");
    EXPECT_EQ(formatHexId(0xFFFFFFFFFFFFFFFFULL), "ffffffffffffffff");
}

TEST(IdGeneratorTest, FormatUuidV4SetsVersionAndVariant)
{
    EXPECT_EQ(formatUuidV4(0, 0), "00000000-0000-4000-8000-000000000000");
    EXPECT_EQ(formatUuidV4(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL), "ffffffff-ffff-4fff-bfff-ffffffffffff");
    EXPECT_EQ(formatUuidV4(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL), "01234567-89ab-4def-bedc-ba9876543210");
}

TEST(IdGeneratorTest, GeneratedIdHasExpectedShape)
{
    const auto identifier = generateId();
    EXPECT_EQ(identifier.size(), GENERATED_ID_LENGTH);
    EXPECT_TRUE(isLowercaseHex(identifier));
}

TEST(IdGeneratorTest, GeneratedUuidHasExpectedShape)
{
    const auto uuid = generateUuid();
    ASSERT_EQ(uuid.size(), UUID_LENGTH);
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');
    EXPECT_EQ(uuid[14], '4');
    EXPECT_TRUE(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b');
}

TEST(IdGeneratorTest, GeneratedIdsAreDistinct)
{
    constexpr int idCount = 10000;
    std::set<std::string> identifiers;
    for (int index = 0; index < idCount; ++index)
        identifiers.insert(generateId());

    EXPECT_EQ(identifiers.size(), static_cast<std::size_t>(idCount));
}

TEST(IdGeneratorTest, GeneratedIdsAreDistinctAcrossThreads)
{
    constexpr int threadCount = 4;
    constexpr int idsPerThread = 2500;

    std::vector<std::vector<std::string>> perThreadIds(threadCount);
    {
        std::vector<std::jthread> workers;
        for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
        {
            workers.emplace_back([&perThreadIds, threadIndex]
            {
                for (int index = 0; index < idsPerThread; ++index)
                    perThreadIds[threadIndex].push_back(generateId());
            });
        }
    }

    std::set<std::string> allIds;
    for (const auto& threadIds : perThreadIds)
        allIds.insert(threadIds.begin(), threadIds.end());

    EXPECT_EQ(allIds.size(), static_cast<std::size_t>(threadCount * idsPerThread));
}
