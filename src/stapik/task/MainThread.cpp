#include "MainThread.hpp"

#include <glib.h>

#include <deque>
#include <mutex>
#include <utility>

namespace stapik::task
{
    namespace
    {
        std::mutex g_queueMutex;
        std::deque<std::function<void()>> g_queue;
        bool g_drainScheduled = false;

        gboolean drain(gpointer)
        {
            std::deque<std::function<void()>> batch;

            {
                const std::lock_guard lock(g_queueMutex);
                batch.swap(g_queue);
                g_drainScheduled = false;
            }

            for (auto& callback : batch)
                callback();

            return G_SOURCE_REMOVE;
        }
    }

    void postToMainThread(std::function<void()> callback)
    {
        {
            const std::lock_guard lock(g_queueMutex);
            g_queue.push_back(std::move(callback));

            if (g_drainScheduled)
                return;

            g_drainScheduled = true;
        }

        g_idle_add(&drain, nullptr);
    }
}
