#pragma once

#include <glib.h>

#include <chrono>
#include <functional>

namespace stapik::task
{
    class DebouncedAction
    {
    public:
        DebouncedAction(std::chrono::milliseconds delay, std::function<void()> action);
        ~DebouncedAction();

        DebouncedAction(const DebouncedAction&) = delete;
        DebouncedAction& operator=(const DebouncedAction&) = delete;

        void trigger();
        void cancel();
        void flush();

        [[nodiscard]] bool pending() const;

    private:
        static gboolean onTimeout(gpointer data);

        std::chrono::milliseconds m_delay;
        std::function<void()> m_action;
        guint m_sourceId = 0;
    };
}
