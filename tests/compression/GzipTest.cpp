#include "stapik/compression/Gzip.hpp"

#include <gtest/gtest.h>

#include <string>

using stapik::compression::GzipError;
using stapik::compression::gzipCompress;
using stapik::compression::gzipDecompress;

namespace
{
    constexpr std::size_t UNLIMITED = 1024 * 1024;

    std::string repetitiveText()
    {
        std::string text;
        for (int index = 0; index < 500; ++index)
            text += "{\"title\":\"Groceries\",\"amount\":12.50},";

        return text;
    }
}

TEST(GzipTest, DecompressReturnsWhatWasCompressed)
{
    const auto text = repetitiveText();

    const auto compressed = gzipCompress(text);
    ASSERT_TRUE(compressed.hasValue());
    const auto restored = gzipDecompress(compressed.value(), UNLIMITED);

    ASSERT_TRUE(restored.hasValue());
    EXPECT_EQ(restored.value(), text);
}

TEST(GzipTest, RepetitiveTextBecomesMuchSmaller)
{
    const auto text = repetitiveText();

    const auto compressed = gzipCompress(text);

    ASSERT_TRUE(compressed.hasValue());
    EXPECT_LT(compressed.value().size() * 5, text.size());
}

TEST(GzipTest, OutputIsAGzipStream)
{
    const auto compressed = gzipCompress("text");

    ASSERT_TRUE(compressed.hasValue());
    EXPECT_EQ(static_cast<unsigned char>(compressed.value().at(0)), 0x1F);
    EXPECT_EQ(static_cast<unsigned char>(compressed.value().at(1)), 0x8B);
}

TEST(GzipTest, EmptyAndBinaryContentRoundTrips)
{
    std::string binary;
    for (int value = 0; value < 256; ++value)
        binary.push_back(static_cast<char>(value));

    for (const std::string& content : { std::string{}, binary })
    {
        const auto restored = gzipDecompress(gzipCompress(content).value(), UNLIMITED);

        ASSERT_TRUE(restored.hasValue());
        EXPECT_EQ(restored.value(), content);
    }
}

TEST(GzipTest, GarbageIsMalformed)
{
    const auto restored = gzipDecompress("this is not gzip data", UNLIMITED);

    ASSERT_FALSE(restored.hasValue());
    EXPECT_EQ(restored.error(), GzipError::Malformed);
}

TEST(GzipTest, TruncatedStreamIsMalformed)
{
    const auto compressed = gzipCompress(repetitiveText()).value();

    const auto restored = gzipDecompress(compressed.substr(0, compressed.size() / 2), UNLIMITED);

    ASSERT_FALSE(restored.hasValue());
    EXPECT_EQ(restored.error(), GzipError::Malformed);
}

TEST(GzipTest, ResultLargerThanTheLimitIsRejected)
{
    const auto text = repetitiveText();
    const auto compressed = gzipCompress(text).value();

    const auto restored = gzipDecompress(compressed, text.size() - 1);

    ASSERT_FALSE(restored.hasValue());
    EXPECT_EQ(restored.error(), GzipError::TooLarge);
}

TEST(GzipTest, ResultExactlyAtTheLimitIsAccepted)
{
    const auto text = repetitiveText();
    const auto compressed = gzipCompress(text).value();

    EXPECT_TRUE(gzipDecompress(compressed, text.size()).hasValue());
}

TEST(GzipTest, EveryErrorHasDescription)
{
    for (const auto error : { GzipError::CompressionFailed, GzipError::Malformed, GzipError::TooLarge })
        EXPECT_FALSE(stapik::compression::describe(error).empty());
}
