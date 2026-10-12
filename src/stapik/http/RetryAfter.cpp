#include "RetryAfter.hpp"

#include "stapik/text/Trim.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <string>

namespace stapik::http
{
    namespace
    {
        constexpr std::array<std::string_view, 12> MONTHS{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

        std::optional<unsigned> monthNumber(const std::string_view name)
        {
            const auto found = std::ranges::find(MONTHS, name);
            if (found == MONTHS.end())
                return std::nullopt;

            return static_cast<unsigned>(found - MONTHS.begin()) + 1U;
        }

        std::optional<std::chrono::seconds> parseDelaySeconds(const std::string& text)
        {
            if (text.empty() || !std::isdigit(static_cast<unsigned char>(text.front())))
                return std::nullopt;

            long long seconds{};
            if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seconds); error != std::errc{} || end != text.data() + text.size())
                return std::nullopt;

            return std::chrono::seconds{ seconds };
        }

        std::optional<std::chrono::seconds> parseHttpDate(const std::string& text, const time::TimePoint now)
        {
            int day{};
            int year{};
            int hour{};
            int minute{};
            int second{};
            char monthName[4]{};
            if (std::sscanf(text.c_str(), "%*3s, %d %3s %d %d:%d:%d GMT", &day, monthName, &year, &hour, &minute, &second) != 6)
                return std::nullopt;

            const auto month = monthNumber(monthName);
            if (!month)
                return std::nullopt;

            const std::chrono::year_month_day date{ std::chrono::year{ year }, std::chrono::month{ *month }, std::chrono::day{ static_cast<unsigned>(day) } };
            if (!date.ok())
                return std::nullopt;

            using namespace std::chrono;
            const auto target = sys_days{ date } + hours{ hour } + minutes{ minute } + seconds{ second };
            return std::max(duration_cast<seconds>(target - now), seconds::zero());
        }
    }

    std::optional<std::chrono::seconds> parseRetryAfter(const std::string_view headerValue, const time::TimePoint now)
    {
        const auto text = text::trim(headerValue);
        if (const auto delay = parseDelaySeconds(text))
            return delay;

        return parseHttpDate(text, now);
    }
}
