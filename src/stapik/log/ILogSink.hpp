#pragma once

#include "LogRecord.hpp"

namespace stapik::log
{
    // A destination of log messages. The logger calls write() under its own lock, so a sink is never entered by two
    // threads at once through the logger and must not log itself. Records below the minimum level never arrive.
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;

        virtual void write(const LogRecord& record) = 0;
    };
}
