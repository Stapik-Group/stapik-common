#include "LogReport.hpp"

#include <algorithm>
#include <format>
#include <iterator>

namespace
{
    constexpr std::string_view platformName()
    {
#if defined(_WIN32)
        return "Windows";
#elif defined(__linux__)
        return "Linux";
#else
        return "Unknown";
#endif
    }
}

namespace stapik::log
{
    std::vector<LogRecord> filterByLevel(const std::vector<LogRecord>& records, const Level minimum)
    {
        std::vector<LogRecord> result;
        std::ranges::copy_if(records, std::back_inserter(result), [minimum](const LogRecord& record)
        {
            return record.level >= minimum;
        });

        return result;
    }

    std::string formatLogText(const std::vector<LogRecord>& records)
    {
        std::string text;
        for (const auto& record : records)
        {
            if (!text.empty())
                text += '\n';

            text += formatRecord(record);
        }

        return text;
    }

    std::string formatBugReport(const ReportHeader& header, const Level minimum, const std::vector<LogRecord>& records)
    {
        const auto shown = filterByLevel(records, minimum);

        return std::format("Application: {} {}\nPlatform: {}\nLog level: {} and above\nRecords: {}\n\n{}\n",
            header.application, header.version, platformName(), levelName(minimum), shown.size(), formatLogText(shown));
    }
}
