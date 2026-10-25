#include "BackgroundTaskRunner.hpp"

#include "Cancellation.hpp"
#include "MainThread.hpp"
#include "ThreadPool.hpp"

#include "stapik/log/Log.hpp"

#include <exception>
#include <utility>

namespace stapik::task
{
    namespace
    {
        void execute(const std::function<void()>& work)
        {
            try
            {
                work();
            }
            catch (const std::exception& exception)
            {
                log::warning("Background task failed: {}", exception.what());
            }
            catch (...)
            {
                log::warning("Background task failed with an unknown exception");
            }
        }
    }

    BackgroundTaskRunner::BackgroundTaskRunner() :
        m_state(std::make_shared<State>())
    {}

    BackgroundTaskRunner::~BackgroundTaskRunner()
    {
        std::unique_lock lock(m_state->mutex);
        m_state->alive.store(false);
        m_state->tasks.clear();
        m_state->stop.request_stop();
        m_state->idle.wait(lock, [this] { return !m_state->draining; });
    }

    void BackgroundTaskRunner::run(std::function<void()> work, std::function<void()> onFinished)
    {
        bool startDraining = false;

        {
            const std::lock_guard lock(m_state->mutex);
            m_state->tasks.emplace_back(std::move(work), std::move(onFinished));
            startDraining = !std::exchange(m_state->draining, true);
        }

        if (!startDraining)
            return;

        try
        {
            ThreadPool::shared().post([state = m_state] { drain(state); });
        }
        catch (...)
        {
            {
                const std::lock_guard lock(m_state->mutex);
                m_state->draining = false;
            }

            m_state->idle.notify_all();
            throw;
        }
    }

    void BackgroundTaskRunner::drain(const std::shared_ptr<State>& state)
    {
        while (true)
        {
            Task task;

            {
                const std::lock_guard lock(state->mutex);
                if (state->tasks.empty())
                {
                    state->draining = false;
                    state->idle.notify_all();
                    return;
                }

                task = std::move(state->tasks.front());
                state->tasks.pop_front();
            }

            {
                const CancellationScope scope(state->stop.get_token());
                execute(task.work);
            }

            if (task.onFinished)
            {
                postToMainThread([state, onFinished = std::move(task.onFinished)]
                {
                    if (state->alive.load())
                        onFinished();
                });
            }
        }
    }
}
