#include "stapik/task/BackgroundTaskRunner.hpp"
#include "stapik/task/Cancellation.hpp"
#include "stapik/task/ThreadPool.hpp"

#include "support/MainLoopPump.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using stapik::task::BackgroundTaskRunner;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;
}

TEST(BackgroundTaskRunnerTest, WorkRunsOnAnotherThreadAndCompletionOnTheCallingThread)
{
    BackgroundTaskRunner runner;
    const auto callerThread = std::this_thread::get_id();
    std::thread::id workThread;
    std::thread::id finishThread;
    std::atomic<bool> finished{ false };

    runner.run(
        [&workThread] { workThread = std::this_thread::get_id(); },
        [&finishThread, &finished]
        {
            finishThread = std::this_thread::get_id();
            finished = true;
        });

    ASSERT_TRUE(pumpUntil([&finished] { return finished.load(); }));
    EXPECT_NE(workThread, callerThread);
    EXPECT_EQ(finishThread, callerThread);
}

TEST(BackgroundTaskRunnerTest, TasksRunInSubmissionOrder)
{
    BackgroundTaskRunner runner;
    std::vector<int> order;
    std::atomic<int> finished{ 0 };

    for (int index = 0; index < 5; ++index)
    {
        runner.run(
            [&order, index] { order.push_back(index); },
            [&finished] { ++finished; });
    }

    ASSERT_TRUE(pumpUntil([&finished] { return finished.load() == 5; }));
    EXPECT_EQ(order, (std::vector<int>{ 0, 1, 2, 3, 4 }));
}

TEST(BackgroundTaskRunnerTest, SubmitDeliversTheResult)
{
    BackgroundTaskRunner runner;
    std::optional<std::string> received;
    bool called = false;

    runner.submit(
        [] { return std::string("computed"); },
        [&received, &called](std::optional<std::string> result)
        {
            received = std::move(result);
            called = true;
        });

    ASSERT_TRUE(pumpUntil([&called] { return called; }));
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, "computed");
}

TEST(BackgroundTaskRunnerTest, ThrowingWorkYieldsNoResultAndDoesNotStopTheRunner)
{
    BackgroundTaskRunner runner;
    bool failedCalled = false;
    bool failedHadResult = true;
    bool laterCalled = false;

    runner.submit(
        []() -> int { throw std::runtime_error("boom"); },
        [&failedCalled, &failedHadResult](std::optional<int> result)
        {
            failedCalled = true;
            failedHadResult = result.has_value();
        });
    runner.submit(
        [] { return 1; },
        [&laterCalled](std::optional<int>) { laterCalled = true; });

    ASSERT_TRUE(pumpUntil([&laterCalled] { return laterCalled; }));
    EXPECT_TRUE(failedCalled);
    EXPECT_FALSE(failedHadResult);
}

TEST(BackgroundTaskRunnerTest, CompletionIsSkippedWhenTheRunnerIsGone)
{
    std::atomic<bool> workDone{ false };
    bool completionCalled = false;

    {
        BackgroundTaskRunner runner;
        runner.run(
            [&workDone] { workDone = true; },
            [&completionCalled] { completionCalled = true; });

        while (!workDone.load())
            std::this_thread::sleep_for(1ms);
    }

    pumpFor(100ms);

    EXPECT_FALSE(completionCalled);
}

TEST(BackgroundTaskRunnerTest, PendingTasksAreDroppedWhenTheRunnerIsDestroyed)
{
    std::atomic<bool> firstStarted{ false };
    std::atomic<bool> secondRan{ false };

    {
        BackgroundTaskRunner runner;
        runner.run([&firstStarted]
        {
            firstStarted = true;
            std::this_thread::sleep_for(100ms);
        });
        runner.run([&secondRan] { secondRan = true; });

        while (!firstStarted.load())
            std::this_thread::sleep_for(1ms);
    }

    EXPECT_FALSE(secondRan.load());
}

TEST(BackgroundTaskRunnerTest, ManyRunnersShareTheLimitedThreadsOfThePool)
{
    constexpr int RUNNERS = 12;

    std::mutex mutex;
    std::set<std::thread::id> threads;
    std::atomic<int> finished{ 0 };
    std::vector<std::unique_ptr<BackgroundTaskRunner>> runners;

    for (int index = 0; index < RUNNERS; ++index)
    {
        runners.push_back(std::make_unique<BackgroundTaskRunner>());
        runners.back()->run(
            [&mutex, &threads]
            {
                {
                    const std::lock_guard lock(mutex);
                    threads.insert(std::this_thread::get_id());
                }

                std::this_thread::sleep_for(20ms);
            },
            [&finished] { ++finished; });
    }

    ASSERT_TRUE(pumpUntil([&finished] { return finished.load() == RUNNERS; }, 5000ms));
    EXPECT_LE(threads.size(), stapik::task::ThreadPool::SHARED_MAX_THREADS);
}

TEST(BackgroundTaskRunnerTest, ASlowTaskDoesNotBlockAnotherRunner)
{
    std::atomic<bool> release{ false };
    std::atomic<bool> slowStarted{ false };
    std::atomic<bool> quickDone{ false };

    {
        BackgroundTaskRunner slow;
        BackgroundTaskRunner quick;

        slow.run([&release, &slowStarted]
        {
            slowStarted = true;

            while (!release.load())
                std::this_thread::sleep_for(1ms);
        });

        while (!slowStarted.load())
            std::this_thread::sleep_for(1ms);

        quick.run([&quickDone] { quickDone = true; });

        EXPECT_TRUE(pumpUntil([&quickDone] { return quickDone.load(); }));

        release = true;
    }
}

TEST(BackgroundTaskRunnerTest, DestroyingTheRunnerRequestsCancellationOfTheRunningTask)
{
    std::atomic<bool> started{ false };
    std::atomic<bool> sawCancellation{ false };
    const auto begin = std::chrono::steady_clock::now();

    {
        BackgroundTaskRunner runner;
        runner.run([&started, &sawCancellation]
        {
            started = true;
            const auto giveUpAt = std::chrono::steady_clock::now() + 5s;

            while (!stapik::task::isCancellationRequested() && std::chrono::steady_clock::now() < giveUpAt)
                std::this_thread::sleep_for(1ms);

            sawCancellation = stapik::task::isCancellationRequested();
        });

        while (!started.load())
            std::this_thread::sleep_for(1ms);
    }

    EXPECT_TRUE(sawCancellation.load());
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 3s);
}

TEST(BackgroundTaskRunnerTest, TaskIsNotCancelledWhileItsRunnerLives)
{
    BackgroundTaskRunner runner;
    std::optional<bool> cancelled;

    runner.submit(
        [] { return stapik::task::isCancellationRequested(); },
        [&cancelled](std::optional<bool> result) { cancelled = result; });

    ASSERT_TRUE(pumpUntil([&cancelled] { return cancelled.has_value(); }));
    EXPECT_EQ(cancelled, std::optional<bool>{ false });
}
