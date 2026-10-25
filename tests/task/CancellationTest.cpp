#include "stapik/task/Cancellation.hpp"

#include <gtest/gtest.h>

#include <stop_token>
#include <thread>

namespace
{
    using stapik::task::cancellationCheck;
    using stapik::task::CancellationScope;
    using stapik::task::isCancellationRequested;
}

TEST(CancellationTest, NothingIsCancelledOutsideAScope)
{
    EXPECT_FALSE(isCancellationRequested());
}

TEST(CancellationTest, ScopeReportsTheStopRequestOfItsToken)
{
    std::stop_source source;
    const CancellationScope scope(source.get_token());

    EXPECT_FALSE(isCancellationRequested());

    source.request_stop();

    EXPECT_TRUE(isCancellationRequested());
}

TEST(CancellationTest, LeavingAScopeRestoresThePreviousToken)
{
    std::stop_source outer;
    std::stop_source inner;
    inner.request_stop();
    const CancellationScope outerScope(outer.get_token());

    {
        const CancellationScope innerScope(inner.get_token());
        EXPECT_TRUE(isCancellationRequested());
    }

    EXPECT_FALSE(isCancellationRequested());
}

TEST(CancellationTest, ScopeAppliesOnlyToItsOwnThread)
{
    std::stop_source source;
    source.request_stop();
    const CancellationScope scope(source.get_token());
    bool otherThreadCancelled = true;

    std::thread other([&otherThreadCancelled] { otherThreadCancelled = isCancellationRequested(); });
    other.join();

    EXPECT_FALSE(otherThreadCancelled);
}

TEST(CancellationTest, CheckWithoutAdditionalConditionFollowsTheScope)
{
    std::stop_source source;
    const CancellationScope scope(source.get_token());
    const auto check = cancellationCheck();

    EXPECT_FALSE(check());

    source.request_stop();

    EXPECT_TRUE(check());
}

TEST(CancellationTest, CheckIsCancelledByTheAdditionalConditionToo)
{
    bool additional = false;
    const auto check = cancellationCheck([&additional] { return additional; });

    EXPECT_FALSE(check());

    additional = true;

    EXPECT_TRUE(check());
}
