#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace stapik::locale
{
    using LocalSeconds = std::chrono::local_seconds;

    [[nodiscard]] LocalSeconds toLocalTime(std::chrono::system_clock::time_point timePoint);
    [[nodiscard]] std::string formatDate(std::chrono::year_month_day date, std::string_view languageCode);
    [[nodiscard]] std::string formatDate(LocalSeconds time, std::string_view languageCode);
    [[nodiscard]] std::string formatTime(LocalSeconds time, std::string_view languageCode, bool withSeconds = false);
    [[nodiscard]] std::string formatDateTime(LocalSeconds time, std::string_view languageCode, bool withSeconds = false);
}
