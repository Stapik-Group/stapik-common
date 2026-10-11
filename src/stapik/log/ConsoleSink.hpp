#pragma once

#include "ILogSink.hpp"

namespace stapik::log
{
    class ConsoleSink final : public ILogSink
    {
    public:
        void write(const LogRecord& record) override;
    };
}
