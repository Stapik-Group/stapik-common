#include "stapik/text/Base64.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using stapik::text::base64Decode;
using stapik::text::base64Encode;

TEST(Base64Test, EncodesRfc4648Vectors)
{
    EXPECT_EQ(base64Encode(""), "");
    EXPECT_EQ(base64Encode("f"), "Zg==");
    EXPECT_EQ(base64Encode("fo"), "Zm8=");
    EXPECT_EQ(base64Encode("foo"), "Zm9v");
    EXPECT_EQ(base64Encode("foob"), "Zm9vYg==");
    EXPECT_EQ(base64Encode("fooba"), "Zm9vYmE=");
    EXPECT_EQ(base64Encode("foobar"), "Zm9vYmFy");
}

TEST(Base64Test, DecodesRfc4648Vectors)
{
    EXPECT_EQ(base64Decode(""), "");
    EXPECT_EQ(base64Decode("Zg=="), "f");
    EXPECT_EQ(base64Decode("Zm8="), "fo");
    EXPECT_EQ(base64Decode("Zm9v"), "foo");
    EXPECT_EQ(base64Decode("Zm9vYg=="), "foob");
    EXPECT_EQ(base64Decode("Zm9vYmE="), "fooba");
    EXPECT_EQ(base64Decode("Zm9vYmFy"), "foobar");
}

TEST(Base64Test, EveryByteValueRoundTrips)
{
    std::string bytes;
    for (int value = 0; value < 256; ++value)
        bytes.push_back(static_cast<char>(value));

    EXPECT_EQ(base64Decode(base64Encode(bytes)), bytes);
}

TEST(Base64Test, RejectsTextWithWrongLength)
{
    EXPECT_FALSE(base64Decode("Zg=").has_value());
    EXPECT_FALSE(base64Decode("Zm9vY").has_value());
}

TEST(Base64Test, RejectsSymbolsOutsideTheAlphabet)
{
    EXPECT_FALSE(base64Decode("Zm9!").has_value());
    EXPECT_FALSE(base64Decode("Zm 9").has_value());
    EXPECT_FALSE(base64Decode("Zm9-").has_value());
}

TEST(Base64Test, RejectsMisplacedPadding)
{
    EXPECT_FALSE(base64Decode("=AAA").has_value());
    EXPECT_FALSE(base64Decode("A=AA").has_value());
    EXPECT_FALSE(base64Decode("Zg=a").has_value());
    EXPECT_FALSE(base64Decode("Zg==Zm9v").has_value());
}
