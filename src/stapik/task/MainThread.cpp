#include "MainThread.hpp"

#include "stapik/log/Log.hpp"

#include <glib.h>

#include <deque>
#include <exception>
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

            // An exception must not leave through the C frames of GLib, and one failing callback must not
            // take the rest of the batch with it.
            for (auto& callback : batch)
            {
                try
                {
                    callback();
                }
                catch (const std::exception& exception)
                {
                    log::error("A callback posted to the main thread failed: {}", exception.what());
                }
                catch (...)
                {
                    log::error("A callback posted to the main thread failed with an unknown exception");
                }
            }

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
