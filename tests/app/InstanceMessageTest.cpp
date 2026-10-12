#include "stapik/app/InstanceMessage.hpp"

#include <gtest/gtest.h>

namespace
{
    namespace fs = std::filesystem;
    using namespace stapik::app;
}

TEST(InstanceMessageTest, FilesSurviveARoundTripInOrder)
{
    const std::vector files{ fs::path("first.txt"), fs::path(u8"zażółć gęślą.txt"), fs::path("dir") / "third.txt" };

    EXPECT_EQ(decodeInstanceMessage(encodeInstanceMessage(files)), files);
}

TEST(InstanceMessageTest, NoFilesIsAValidMessage)
{
    const auto decoded = decodeInstanceMessage(encodeInstanceMessage({}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->empty());
}

TEST(InstanceMessageTest, RejectsDataWithoutTheHeader)
{
    EXPECT_FALSE(decodeInstanceMessage("").has_value());
    EXPECT_FALSE(decodeInstanceMessage("file.txt\n").has_value());
    EXPECT_FALSE(decodeInstanceMessage("stapik-instance/2\nfile.txt\n").has_value());
}

TEST(InstanceMessageTest, SkipsBlankLinesAndAcceptsMissingTrailingNewline)
{
    const auto decoded = decodeInstanceMessage(std::string(INSTANCE_MESSAGE_HEADER) + "a.txt\n\nb.txt");

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, (std::vector{ fs::path("a.txt"), fs::path("b.txt") }));
}
