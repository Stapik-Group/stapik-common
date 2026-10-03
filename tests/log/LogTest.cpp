#include "stapik/log/Log.hpp"

#include <glib.h>
#include <gtest/gtest.h>

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
            m_handlerId = g_log_set_handler("stapik", G_LOG_LEVEL_MASK, &LogTest::collect, &m_messages);
        }

        void TearDown() override
        {
            g_log_remove_handler("stapik", m_handlerId);
            stapik::log::setLevel(m_originalLevel);
        }

        std::vector<std::string> m_messages;

    private:
        static void collect(const gchar*, GLogLevelFlags, const gchar* message, gpointer userData)
        {
            static_cast<std::vector<std::string>*>(userData)->emplace_back(message);
        }

        stapik::log::Level m_originalLevel{};
        guint m_handlerId{};
    };
}

TEST_F(LogTest, FormatsMessagesWithStdFormat)
{
    stapik::log::setLevel(stapik::log::Level::Info);

    stapik::log::warning("value {} and {}", 42, "text");

    ASSERT_EQ(m_messages.size(), 1u);
    EXPECT_EQ(m_messages.front(), "value 42 and text");
}

TEST_F(LogTest, MessagesBelowMinimumLevelAreDropped)
{
    stapik::log::setLevel(stapik::log::Level::Warning);

    stapik::log::debug("debug");
    stapik::log::info("info");
    stapik::log::warning("warning");
    stapik::log::error("error");

    ASSERT_EQ(m_messages.size(), 2u);
    EXPECT_EQ(m_messages[0], "warning");
    EXPECT_EQ(m_messages[1], "error");
}

TEST_F(LogTest, SetLevelIsReflectedByLevel)
{
    stapik::log::setLevel(stapik::log::Level::Error);

    EXPECT_EQ(stapik::log::level(), stapik::log::Level::Error);
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
