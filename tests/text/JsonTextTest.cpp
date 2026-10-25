#include "stapik/text/JsonText.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    constexpr auto REPLACEMENT_CHARACTER = "\xEF\xBF\xBD";
}

TEST(JsonTextTest, ValidTextIsDumpedLikeThePlainDump)
{
    const nlohmann::json json = { { "title", "Zażółć gęślą jaźń" }, { "items", { 1, 2, 3 } } };

    EXPECT_EQ(stapik::text::dumpJson(json), json.dump());
    EXPECT_EQ(stapik::text::dumpJson(json, 2), json.dump(2));
}

TEST(JsonTextTest, DumpIsCompactByDefault)
{
    const nlohmann::json json = { { "a", 1 } };

    EXPECT_EQ(stapik::text::dumpJson(json), R"({"a":1})");
}

TEST(JsonTextTest, InvalidUtf8InAValueIsReplacedInsteadOfThrowing)
{
    const nlohmann::json json = { { "name", std::string("bad\xff name") } };

    std::string text;
    EXPECT_NO_THROW(text = stapik::text::dumpJson(json));

    EXPECT_EQ(nlohmann::json::parse(text).at("name").get<std::string>(), std::string("bad") + REPLACEMENT_CHARACTER + " name");
}

TEST(JsonTextTest, InvalidUtf8InAKeyIsReplacedInsteadOfThrowing)
{
    nlohmann::json json = nlohmann::json::object();
    json[std::string("key\xc3")] = 1;

    std::string text;
    EXPECT_NO_THROW(text = stapik::text::dumpJson(json, 2));

    EXPECT_TRUE(nlohmann::json::parse(text).contains(std::string("key") + REPLACEMENT_CHARACTER));
}

TEST(JsonTextTest, TruncatedSequenceAtTheEndIsReplaced)
{
    const nlohmann::json json = std::string("end\xe2\x82");

    EXPECT_EQ(nlohmann::json::parse(stapik::text::dumpJson(json)).get<std::string>(), std::string("end") + REPLACEMENT_CHARACTER);
}

TEST(JsonTextTest, ValidMultiByteTextIsKeptUnchanged)
{
    const nlohmann::json json = std::string("\xE2\x82\xAC 5");

    EXPECT_EQ(nlohmann::json::parse(stapik::text::dumpJson(json)).get<std::string>(), "\xE2\x82\xAC 5");
}
