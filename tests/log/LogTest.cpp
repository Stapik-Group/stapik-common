#include "stapik/log/Log.hpp"

#include "stapik/log/RingBufferSink.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    class LogTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_originalLevel = stapik::log::level();
            m_originalSinks = stapik::log::sinks();
            m_sink = std::make_shared<stapik::log::RingBufferSink>(100);
            stapik::log::setSinks({m_sink});
        }

        void TearDown() override
        {
            stapik::log::setSinks(m_originalSinks);
            stapik::log::setLevel(m_originalLevel);
        }

        [[nodiscard]] std::vector<std::string> messages() const
        {
            std::vector<std::string> result;
            for (const auto& record : m_sink->snapshot())
                result.push_back(record.message);

            return result;
        }

        std::shared_ptr<stapik::log::RingBufferSink> m_sink;

    private:
        stapik::log::Level m_originalLevel{};
        std::vector<std::shared_ptr<stapik::log::ILogSink>> m_originalSinks;
    };
}

TEST_F(LogTest, FormatsMessagesWithStdFormat)
{
    stapik::log::setLevel(stapik::log::Level::Info);

    stapik::log::warning("value {} and {}", 42, "text");

    ASSERT_EQ(messages().size(), 1u);
    EXPECT_EQ(messages().front(), "value 42 and text");
}

TEST_F(LogTest, MessagesBelowMinimumLevelAreDropped)
{
    stapik::log::setLevel(stapik::log::Level::Warning);

    stapik::log::debug("debug");
    stapik::log::info("info");
    stapik::log::warning("warning");
    stapik::log::error("error");

    ASSERT_EQ(messages().size(), 2u);
    EXPECT_EQ(messages()[0], "warning");
    EXPECT_EQ(messages()[1], "error");
}

TEST_F(LogTest, SetLevelIsReflectedByLevel)
{
    stapik::log::setLevel(stapik::log::Level::Error);

    EXPECT_EQ(stapik::log::level(), stapik::log::Level::Error);
}

TEST_F(LogTest, RecordsCarryTheirLevel)
{
    stapik::log::setLevel(stapik::log::Level::Debug);

    stapik::log::debug("first");
    stapik::log::error("second");

    const auto records = m_sink->snapshot();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].level, stapik::log::Level::Debug);
    EXPECT_EQ(records[1].level, stapik::log::Level::Error);
}

TEST_F(LogTest, EveryRegisteredSinkReceivesTheRecord)
{
    const auto second = std::make_shared<stapik::log::RingBufferSink>(10);
    stapik::log::addSink(second);

    stapik::log::warning("shared");

    EXPECT_EQ(m_sink->snapshot().size(), 1u);
    EXPECT_EQ(second->snapshot().size(), 1u);
}

TEST_F(LogTest, SinkListCanBeReplaced)
{
    const auto replacement = std::make_shared<stapik::log::RingBufferSink>(10);
    stapik::log::setSinks({replacement});

    stapik::log::warning("only here");

    EXPECT_TRUE(m_sink->snapshot().empty());
    EXPECT_EQ(replacement->snapshot().size(), 1u);
}

TEST(LogRedactTest, HidesShortSecretsCompletely)
{
    EXPECT_EQ(stapik::log::redact(""), "...");
    EXPECT_EQ(stapik::log::redact("abc"), "...");
    EXPECT_EQ(stapik::log::redact("12345678"), "...");
}

TEST(LogRedactTest, ShowsOnlyPrefixOfLongSecrets)
{
    EXPECT_EQ(stapik::log::redact("123456789"), "1234...");
    EXPECT_EQ(stapik::log::redact("abcdefghijklmnop"), "abcd...");
}

TEST(LogParseLevelTest, RecognizesLevelNamesAndAliasesIgnoringCase)
{
    using stapik::log::Level;
    using stapik::log::parseLevel;

    EXPECT_EQ(parseLevel("debug"), Level::Debug);
    EXPECT_EQ(parseLevel("INFO"), Level::Info);
    EXPECT_EQ(parseLevel("Warn"), Level::Warning);
    EXPECT_EQ(parseLevel("warning"), Level::Warning);
    EXPECT_EQ(parseLevel("err"), Level::Error);
    EXPECT_EQ(parseLevel("error"), Level::Error);
}

TEST(LogParseLevelTest, RejectsUnknownNames)
{
    EXPECT_FALSE(stapik::log::parseLevel("").has_value());
    EXPECT_FALSE(stapik::log::parseLevel("verbose").has_value());
}
