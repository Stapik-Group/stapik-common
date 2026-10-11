#include "stapik/log/GLibLogBridge.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/log/RingBufferSink.hpp"

#include <glib.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace
{
    class GLibLogBridgeTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_originalSinks = stapik::log::sinks();
            m_sink = std::make_shared<stapik::log::RingBufferSink>(10);
            stapik::log::setSinks({m_sink});
            m_originalHandler = g_log_set_default_handler(g_log_default_handler, nullptr);
            stapik::log::forwardGLibMessagesToLog();
        }

        void TearDown() override
        {
            g_log_set_default_handler(m_originalHandler, nullptr);
            stapik::log::setSinks(m_originalSinks);
        }

        std::shared_ptr<stapik::log::RingBufferSink> m_sink;

    private:
        std::vector<std::shared_ptr<stapik::log::ILogSink>> m_originalSinks;
        GLogFunc m_originalHandler = nullptr;
    };
}

TEST_F(GLibLogBridgeTest, GLibWarningsReachTheSinksWithTheirDomain)
{
    g_log("Gtk", G_LOG_LEVEL_WARNING, "%s", "something odd");

    const auto records = m_sink->snapshot();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records.front().level, stapik::log::Level::Warning);
    EXPECT_EQ(records.front().message, "Gtk: something odd");
}

TEST_F(GLibLogBridgeTest, CriticalMessagesAreErrors)
{
    g_log("Gtk", G_LOG_LEVEL_CRITICAL, "%s", "broken");

    ASSERT_EQ(m_sink->snapshot().size(), 1u);
    EXPECT_EQ(m_sink->snapshot().front().level, stapik::log::Level::Error);
}
