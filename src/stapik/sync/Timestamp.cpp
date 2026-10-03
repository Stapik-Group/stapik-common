#include "Timestamp.hpp"

#include <charconv>
#include <format>

namespace
{
    bool consumeNumber(std::string_view &text, const std::size_t digits, int &value)
    {
        if (text.size() < digits)
            return false;

        for (std::size_t index = 0; index < digits; ++index)
        {
            if (text[index] < '0' || text[index] > '9')
                return false;
        }

        if (const auto [end, errorCode] = std::from_chars(text.data(), text.data() + digits, value); errorCode != std::errc{} || end != text.data() + digits)
            return false;

        text.remove_prefix(digits);
        return true;
    }

    bool consumeCharacter(std::string_view &text, const char expected)
    {
        if (text.empty() || text.front() != expected)
            return false;

        text.remove_prefix(1);
        return true;
    }
}

namespace stapik::sync
{
    std::string toIso8601(const std::chrono::system_clock::time_point tp, const TimestampPrecision precision)
    {
        if (precision == TimestampPrecision::Microseconds)
            return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::microseconds>(tp));

        return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(tp));
    }

    std::optional<std::chrono::system_clock::time_point> parseIso8601(std::string_view text)
    {
        using namespace std::chrono;
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;

        if (!consumeNumber(text, 4, year) || !consumeCharacter(text, '-') ||
            !consumeNumber(text, 2, month) || !consumeCharacter(text, '-') ||
            !consumeNumber(text, 2, day) ||
            !(consumeCharacter(text, 'T') || consumeCharacter(text, ' ')) ||
            !consumeNumber(text, 2, hour) || !consumeCharacter(text, ':') ||
            !consumeNumber(text, 2, minute) || !consumeCharacter(text, ':') ||
            !consumeNumber(text, 2, second))
        {
            return std::nullopt;
        }

        const year_month_day date{
            std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
            std::chrono::day{static_cast<unsigned>(day)}
        };
        if (!date.ok() || hour > 23 || minute > 59 || second > 59)
            return std::nullopt;

        microseconds fraction{0};
        if (consumeCharacter(text, '.'))
        {
            constexpr std::size_t MICROSECOND_DIGITS = 6;

            std::size_t digits = 0;
            long long micros = 0;
            while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9')
            {
                if (digits < MICROSECOND_DIGITS)
                    micros = micros * 10 + (text[digits] - '0');
                ++digits;
            }

            if (constexpr std::size_t MAX_FRACTION_DIGITS = 9; digits == 0 || digits > MAX_FRACTION_DIGITS)
                return std::nullopt;

            for (std::size_t padding = digits; padding < MICROSECOND_DIGITS; ++padding)
                micros *= 10;

            fraction = microseconds{micros};
            text.remove_prefix(digits);
        }

        minutes offset{0};
        if (consumeCharacter(text, 'Z'))
        {
            // UTC
        } else if (!text.empty() && (text.front() == '+' || text.front() == '-'))
        {
            const bool negative = text.front() == '-';
            text.remove_prefix(1);

            int offsetHours = 0;
            int offsetMinutes = 0;
            if (!consumeNumber(text, 2, offsetHours) || !consumeCharacter(text, ':')
                || !consumeNumber(text, 2, offsetMinutes) || offsetHours > 23 || offsetMinutes > 59)
            {
                return std::nullopt;
            }

            offset = hours{offsetHours} + minutes{offsetMinutes};
            if (negative)
                offset = -offset;
        }

        if (!text.empty())
            return std::nullopt;

        const auto utcTime = sys_days{date} + hours{hour} + minutes{minute} + seconds{second} + fraction - offset;
        return time_point_cast<system_clock::duration>(utcTime);
    }

    std::chrono::system_clock::time_point fromIso8601(const std::string &str)
    {
        return parseIso8601(str).value_or(std::chrono::system_clock::time_point{});
    }
}
