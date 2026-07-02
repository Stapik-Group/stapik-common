#include "Timestamp.hpp"

#include <format>

namespace stapik::sync
{
    std::string toIso8601(const std::chrono::system_clock::time_point tp)
    {
        const auto days = std::chrono::floor<std::chrono::days>(tp);
        const std::chrono::year_month_day ymd{days};
        const std::chrono::hh_mm_ss hms{std::chrono::floor<std::chrono::seconds>(tp - days)};

        return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
            static_cast<int>(ymd.year()),
            static_cast<unsigned>(ymd.month()),
            static_cast<unsigned>(ymd.day()),
            hms.hours().count(),
            hms.minutes().count(),
            hms.seconds().count());
    }

    std::chrono::system_clock::time_point fromIso8601(const std::string& str)
    {
        if (str.size() < 19)
            return {};

        const int year = std::stoi(str.substr(0, 4));
        const auto month = static_cast<unsigned>(std::stoi(str.substr(5, 2)));
        const auto day = static_cast<unsigned>(std::stoi(str.substr(8, 2)));
        const auto hour = std::stoi(str.substr(11, 2));
        const auto minute = std::stoi(str.substr(14, 2));
        const auto second = std::stoi(str.substr(17, 2));

        const std::chrono::year_month_day ymd{
            std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}
        };

        return std::chrono::sys_days{ymd} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second};
    }
}