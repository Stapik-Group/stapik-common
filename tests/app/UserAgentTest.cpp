#include "stapik/app/UserAgent.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace stapik::app;
}

TEST(UserAgentTest, ContainsTheApplicationNameAndVersion)
{
    EXPECT_EQ(userAgentOf(AppInfo{ .internalName = "budgeting", .version = "1.2.0" }), "budgeting/1.2.0");
}

TEST(UserAgentTest, VersionIsLeftOutWhenUnknown)
{
    EXPECT_EQ(userAgentOf(AppInfo{ .internalName = "budgeting" }), "budgeting");
}

TEST(UserAgentTest, FallsBackToTheLibraryNameWithoutAnApplicationName)
{
    EXPECT_EQ(userAgentOf(AppInfo{ .version = "1.2.0" }), "stapik-common");
}
