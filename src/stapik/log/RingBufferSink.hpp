#pragma once

#include "ILogSink.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace stapik::log
{
    // Keeps the newest records in memory, for example to show them in an about or diagnostics dialog.
    class RingBufferSink final : public ILogSink
    {
    public:
        explicit RingBufferSink(std::size_t capacity);

        void write(const LogRecord& record) override;

        // Oldest first. Safe to call from any thread.
        [[nodiscard]] std::vector<LogRecord> snapshot() const;
        void clear();

    private:
        std::size_t m_capacity;
        mutable std::mutex m_mutex;
        std::deque<LogRecord> m_records;
    };
}
