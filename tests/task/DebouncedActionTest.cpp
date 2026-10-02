#include "stapik/task/DebouncedAction.hpp"

#include "support/MainLoopPump.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace
{
    using namespace std::chrono_literals;
    using stapik::task::DebouncedAction;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;
}

TEST(DebouncedActionTest, FiresOnceAfterTheDelay)
{
    int calls = 0;
    DebouncedAction action(50ms, [&calls] { ++calls; });

    action.trigger();
    EXPECT_TRUE(action.pending());
    EXPECT_EQ(calls, 0);

    ASSERT_TRUE(pumpUntil([&calls] { return calls > 0; }));
    pumpFor(100ms);

    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(action.pending());
}

TEST(DebouncedActionTest, RepeatedTriggersCollapseIntoOneCall)
{
    int calls = 0;
    DebouncedAction action(50ms, [&calls] { ++calls; });

    for (int index = 0; index < 5; ++index)
        action.trigger();

    pumpFor(250ms);

    EXPECT_EQ(calls, 1);
}

TEST(DebouncedActionTest, TriggeringAgainRestartsTheTimer)
{
    int calls = 0;
    DebouncedAction action(250ms, [&calls] { ++calls; });

    action.trigger();
    pumpFor(150ms);
    action.trigger();
    pumpFor(180ms);

    EXPECT_EQ(calls, 0);
    ASSERT_TRUE(pumpUntil([&calls] { return calls > 0; }));
    EXPECT_EQ(calls, 1);
}

TEST(DebouncedActionTest, CancelPreventsTheCall)
{
    int calls = 0;
    DebouncedAction action(30ms, [&calls] { ++calls; });

    action.trigger();
    action.cancel();
    pumpFor(150ms);

    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(action.pending());
}

TEST(DebouncedActionTest, FlushRunsPendingActionImmediatelyOnce)
{
    int calls = 0;
    DebouncedAction action(500ms, [&calls] { ++calls; });

    action.trigger();
    action.flush();

    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(action.pending());

    pumpFor(100ms);
    EXPECT_EQ(calls, 1);
}

TEST(DebouncedActionTest, FlushWithoutPendingTriggerDoesNothing)
{
    int calls = 0;
    DebouncedAction action(30ms, [&calls] { ++calls; });

    action.flush();

    EXPECT_EQ(calls, 0);
}

TEST(DebouncedActionTest, DestroyingCancelsThePendingCall)
{
    int calls = 0;

    {
        DebouncedAction action(30ms, [&calls] { ++calls; });
        action.trigger();
    }

    pumpFor(150ms);

    EXPECT_EQ(calls, 0);
}

TEST(DebouncedActionTest, ActionMayTriggerItselfAgain)
{
    int calls = 0;
    DebouncedAction* self = nullptr;
    DebouncedAction action(20ms, [&calls, &self]
    {
        if (++calls < 3)
            self->trigger();
    });
    self = &action;

    action.trigger();

    ASSERT_TRUE(pumpUntil([&calls] { return calls == 3; }));
    pumpFor(100ms);
    EXPECT_EQ(calls, 3);
}

TEST(DebouncedActionTest, NewDelayAppliesToTheNextTrigger)
{
    int calls = 0;
    DebouncedAction action(2000ms, [&calls] { ++calls; });

    action.setDelay(30ms);
    action.trigger();

    ASSERT_TRUE(pumpUntil([&calls] { return calls > 0; }, 500ms));
    EXPECT_EQ(calls, 1);
}
