#include "ThreadPool.hpp"

#include "stapik/log/Log.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace stapik::task
{
    namespace
    {
        void runGuarded(const std::function<void()>& job)
        {
            try
            {
                job();
            }
            catch (const std::exception& exception)
            {
                log::error("A thread pool job failed: {}", exception.what());
            }
            catch (...)
            {
                log::error("A thread pool job failed with an unknown exception");
            }
        }
    }

    ThreadPool::ThreadPool(const std::size_t maxThreads) :
        m_maxThreads(std::max<std::size_t>(maxThreads, 1))
    {}

    ThreadPool::~ThreadPool() = default;

    ThreadPool& ThreadPool::shared()
    {
        static ThreadPool pool(SHARED_MAX_THREADS);
        return pool;
    }

    void ThreadPool::post(std::function<void()> job)
    {
        const std::lock_guard lock(m_mutex);
        m_jobs.push_back(std::move(job));

        if (m_jobs.size() > m_idleWorkers && m_workers.size() < m_maxThreads)
            m_workers.emplace_back([this](const std::stop_token& stopToken) { workerLoop(stopToken); });

        m_wake.notify_one();
    }

    std::size_t ThreadPool::threadCount() const
    {
        const std::lock_guard lock(m_mutex);
        return m_workers.size();
    }

    void ThreadPool::workerLoop(const std::stop_token& stopToken)
    {
        std::unique_lock lock(m_mutex);

        while (true)
        {
            ++m_idleWorkers;
            m_wake.wait(lock, stopToken, [this] { return !m_jobs.empty(); });
            --m_idleWorkers;

            if (stopToken.stop_requested() || m_jobs.empty())
                return;

            auto job = std::move(m_jobs.front());
            m_jobs.pop_front();

            lock.unlock();
            runGuarded(job);
            lock.lock();
        }
    }
}
