#include "ConsoleSink.hpp"

#include <cstdio>

namespace stapik::log
{
    void ConsoleSink::write(const LogRecord& record)
    {
        const auto line = formatRecord(record) + '\n';
        std::fputs(line.c_str(), stderr);
    }
}
