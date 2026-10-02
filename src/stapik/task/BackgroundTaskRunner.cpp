#include "BackgroundTaskRunner.hpp"

#include "MainThread.hpp"

#include "stapik/log/Log.hpp"

#include <exception>

namespace stapik::task
{
    BackgroundTaskRunner::BackgroundTaskRunner() :
        m_alive(std::make_shared<std::atomic<bool>>(true)),
        m_worker([this](std::stop_token stopToken) { workerLoop(stopToken); })
    {}

    BackgroundTaskRunner::~BackgroundTaskRunner()
    {
        m_alive->store(false);
        m_worker.request_stop();
    }

    void BackgroundTaskRunner::run(std::function<void()> work, std::function<void()> onFinished)
    {
        {
            const std::lock_guard lock(m_mutex);
            m_tasks.push_back({ std::move(work), std::move(onFinished) });
        }

        m_wake.notify_one();
    }

    void BackgroundTaskRunner::workerLoop(std::stop_token stopToken)
    {
        while (true)
        {
            Task task;

            {
                std::unique_lock lock(m_mutex);
                m_wake.wait(lock, stopToken, [this] { return !m_tasks.empty(); });

                if (stopToken.stop_requested())
                    return;

                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }

            try
            {
                task.work();
            }
            catch (const std::exception& exception)
            {
                stapik::log::warning("Background task failed: {}", exception.what());
            }
            catch (...)
            {
                stapik::log::warning("Background task failed with an unknown exception");
            }

            if (task.onFinished)
            {
                postToMainThread([alive = m_alive, onFinished = std::move(task.onFinished)]
                {
                    if (alive->load())
                        onFinished();
                });
            }
        }
    }
}
