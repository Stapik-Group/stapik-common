#pragma once

#include "LogLevel.hpp"
#include "LogRecord.hpp"

#include <string>
#include <vector>

namespace stapik::log
{
    struct ReportHeader
    {
        std::string application;
        std::string version;
    };

    [[nodiscard]] std::vector<LogRecord> filterByLevel(const std::vector<LogRecord>& records, Level minimum);
    [[nodiscard]] std::string formatLogText(const std::vector<LogRecord>& records);
    [[nodiscard]] std::string formatBugReport(const ReportHeader& header, Level minimum, const std::vector<LogRecord>& records);
}
