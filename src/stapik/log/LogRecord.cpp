#include "LogRecord.hpp"

#include <format>

namespace stapik::log
{
    std::string formatRecord(const LogRecord& record)
    {
        const auto seconds = std::chrono::floor<std::chrono::seconds>(record.timestamp);
        return std::format("{:%F %T} [{}] {}", seconds, levelName(record.level), record.message);
    }
}
