#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace stapik::task
{
    class ThreadPool
    {
    public:
        static constexpr std::size_t SHARED_MAX_THREADS = 4;

        explicit ThreadPool(std::size_t maxThreads);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        [[nodiscard]] static ThreadPool& shared();

        void post(std::function<void()> job);

        [[nodiscard]] std::size_t threadCount() const;

    private:
        void workerLoop(const std::stop_token& stopToken);

        std::size_t m_maxThreads;
        mutable std::mutex m_mutex;
        std::condition_variable_any m_wake;
        std::deque<std::function<void()>> m_jobs;
        std::size_t m_idleWorkers = 0;
        std::vector<std::jthread> m_workers;
    };
}
