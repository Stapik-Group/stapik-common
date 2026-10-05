#include "stapik/storage/PathText.hpp"

#include <gtest/gtest.h>

namespace
{
    using stapik::storage::pathText;
}

TEST(PathTextTest, AsciiPathIsReturnedUnchanged)
{
    EXPECT_EQ(pathText(std::filesystem::path("/home/user/app/data.json")), "/home/user/app/data.json");
}

TEST(PathTextTest, EmptyPathGivesEmptyText)
{
    EXPECT_EQ(pathText(std::filesystem::path()), "");
}

TEST(PathTextTest, NonAsciiCharactersComeOutAsUtf8)
{
    const std::filesystem::path path = std::filesystem::path(u8"/home/zażółć") / u8"gęślą.json";

    EXPECT_EQ(pathText(path), "/home/zażółć/gęślą.json");
}
