#include "RingBufferSink.hpp"

namespace stapik::log
{
    RingBufferSink::RingBufferSink(const std::size_t capacity) : m_capacity(capacity)
    {}

    void RingBufferSink::write(const LogRecord& record)
    {
        const std::lock_guard lock(m_mutex);

        if (m_capacity == 0)
            return;

        if (m_records.size() == m_capacity)
            m_records.pop_front();

        m_records.push_back(record);
    }

    std::vector<LogRecord> RingBufferSink::snapshot() const
    {
        const std::lock_guard lock(m_mutex);
        return {m_records.begin(), m_records.end()};
    }

    void RingBufferSink::clear()
    {
        const std::lock_guard lock(m_mutex);
        m_records.clear();
    }
}
