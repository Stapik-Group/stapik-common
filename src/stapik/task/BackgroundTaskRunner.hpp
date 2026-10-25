#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace stapik::task
{
    class BackgroundTaskRunner
    {
    public:
        BackgroundTaskRunner();
        ~BackgroundTaskRunner();

        BackgroundTaskRunner(const BackgroundTaskRunner&) = delete;
        BackgroundTaskRunner& operator=(const BackgroundTaskRunner&) = delete;

        void run(std::function<void()> work, std::function<void()> onFinished = {});

        template<typename Work, typename Handler>
        void submit(Work work, Handler onResult)
        {
            using Result = std::invoke_result_t<Work>;

            auto slot = std::make_shared<std::optional<Result>>();

            run(
                [work = std::move(work), slot]
                {
                    *slot = work();
                },
                [slot, onResult = std::move(onResult)]
                {
                    onResult(std::move(*slot));
                });
        }

    private:
        struct Task
        {
            std::function<void()> work;
            std::function<void()> onFinished;
        };

        struct State
        {
            std::mutex mutex;
            std::condition_variable idle;
            std::deque<Task> tasks;
            bool draining = false;
            std::atomic<bool> alive{ true };
            std::stop_source stop;
        };

        static void drain(const std::shared_ptr<State>& state);

        std::shared_ptr<State> m_state;
    };
}
