#include "stapik/task/ThreadPool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

namespace
{
    using namespace std::chrono_literals;
    using stapik::task::ThreadPool;

    void waitUntil(const std::atomic<int>& counter, const int expected)
    {
        const auto deadline = std::chrono::steady_clock::now() + 3s;

        while (counter.load() < expected && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
    }
}

TEST(ThreadPoolTest, RunsEveryPostedJob)
{
    ThreadPool pool(2);
    std::atomic<int> done{ 0 };

    for (int index = 0; index < 10; ++index)
        pool.post([&done] { ++done; });

    waitUntil(done, 10);

    EXPECT_EQ(done.load(), 10);
}

TEST(ThreadPoolTest, NeverStartsMoreThreadsThanAllowed)
{
    ThreadPool pool(2);
    std::atomic<int> done{ 0 };
    std::mutex mutex;
    std::set<std::thread::id> threads;

    for (int index = 0; index < 8; ++index)
    {
        pool.post([&]
        {
            {
                const std::lock_guard lock(mutex);
                threads.insert(std::this_thread::get_id());
            }

            std::this_thread::sleep_for(20ms);
            ++done;
        });
    }

    waitUntil(done, 8);

    EXPECT_EQ(done.load(), 8);
    EXPECT_LE(threads.size(), 2U);
    EXPECT_LE(pool.threadCount(), 2U);
}

TEST(ThreadPoolTest, ThrowingJobDoesNotStopTheWorker)
{
    ThreadPool pool(1);
    std::atomic<int> done{ 0 };

    pool.post([] { throw std::runtime_error("boom"); });
    pool.post([&done] { ++done; });

    waitUntil(done, 1);

    EXPECT_EQ(done.load(), 1);
}
