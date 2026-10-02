#include "stapik/text/Trim.hpp"

#include <gtest/gtest.h>

TEST(TrimTest, RemovesLeadingAndTrailingWhitespace)
{
    EXPECT_EQ(stapik::text::trim("  hello \t\r\n"), "hello");
}

TEST(TrimTest, KeepsInnerWhitespace)
{
    EXPECT_EQ(stapik::text::trim(" a  b "), "a  b");
}

TEST(TrimTest, BlankInputBecomesEmpty)
{
    EXPECT_TRUE(stapik::text::trim("").empty());
    EXPECT_TRUE(stapik::text::trim(" \t\r\n ").empty());
}

TEST(TrimTest, LeavesTrimmedTextUntouched)
{
    EXPECT_EQ(stapik::text::trim("hello"), "hello");
}
