#include "DebouncedAction.hpp"

#include <utility>

namespace stapik::task
{
    DebouncedAction::DebouncedAction(const std::chrono::milliseconds delay, std::function<void()> action) :
        m_delay(delay),
        m_action(std::move(action))
    {}

    DebouncedAction::~DebouncedAction()
    {
        cancel();
    }

    void DebouncedAction::trigger()
    {
        cancel();
        m_sourceId = g_timeout_add(static_cast<guint>(m_delay.count()), &DebouncedAction::onTimeout, this);
    }

    void DebouncedAction::cancel()
    {
        if (m_sourceId == 0)
            return;

        g_source_remove(m_sourceId);
        m_sourceId = 0;
    }

    void DebouncedAction::flush()
    {
        if (m_sourceId == 0)
            return;

        cancel();

        if (m_action)
            m_action();
    }

    bool DebouncedAction::pending() const
    {
        return m_sourceId != 0;
    }

    gboolean DebouncedAction::onTimeout(gpointer data)
    {
        auto* self = static_cast<DebouncedAction*>(data);
        self->m_sourceId = 0;

        if (self->m_action)
            self->m_action();

        return G_SOURCE_REMOVE;
    }
}
