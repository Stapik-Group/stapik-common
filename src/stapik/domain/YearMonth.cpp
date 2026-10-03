#include "YearMonth.hpp"

#include "stapik/locale/LocaleManager.hpp"

#include <array>
#include <charconv>
#include <ctime>
#include <format>
#include <stdexcept>

namespace stapik::domain
{
    namespace
    {
        constexpr std::size_t KEY_YEAR_DIGITS = 4;
        constexpr std::size_t KEY_MONTH_DIGITS = 2;
        constexpr std::size_t KEY_LENGTH = KEY_YEAR_DIGITS + 1 + KEY_MONTH_DIGITS;

        bool isValid(const int year, const int month)
        {
            return year >= MIN_YEAR_MONTH_YEAR && year <= MAX_YEAR_MONTH_YEAR && month >= 1 && month <= MONTHS_PER_YEAR;
        }

        bool parseDigits(const std::string_view digits, int& value)
        {
            for (const char digit : digits)
            {
                if (digit < '0' || digit > '9')
                    return false;
            }

            const auto [end, errorCode] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
            return errorCode == std::errc() && end == digits.data() + digits.size();
        }

        constexpr std::array<std::string_view, MONTHS_PER_YEAR> MONTH_NAME_KEYS = {
            "month.january", "month.february", "month.march", "month.april", "month.may", "month.june",
            "month.july", "month.august", "month.september", "month.october", "month.november", "month.december"};

        // Months counted from year 0, so that arithmetic works without special cases.
        int toMonthIndex(const int year, const int month)
        {
            return year * MONTHS_PER_YEAR + (month - 1);
        }
    }

    YearMonth::YearMonth(const int year, const int month) :
        m_year(year),
        m_month(month)
    {
        if (!isValid(year, month))
            throw std::invalid_argument(std::format("Invalid year/month: {}-{}", year, month));
    }

    std::optional<YearMonth> YearMonth::tryCreate(const int year, const int month)
    {
        if (!isValid(year, month))
            return std::nullopt;

        return YearMonth(year, month);
    }

    std::optional<YearMonth> YearMonth::fromKey(const std::string_view key)
    {
        if (key.size() != KEY_LENGTH || key[KEY_YEAR_DIGITS] != '-')
            return std::nullopt;

        int year = 0;
        int month = 0;
        if (!parseDigits(key.substr(0, KEY_YEAR_DIGITS), year) || !parseDigits(key.substr(KEY_YEAR_DIGITS + 1), month))
            return std::nullopt;

        return tryCreate(year, month);
    }

    YearMonth YearMonth::fromDate(const std::chrono::year_month_day& date)
    {
        return YearMonth(static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())));
    }

    YearMonth YearMonth::current()
    {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_r(&now, &localTime);
        return YearMonth(localTime.tm_year + 1900, localTime.tm_mon + 1);
    }

    int YearMonth::year() const
    {
        return m_year;
    }

    int YearMonth::month() const
    {
        return m_month;
    }

    int YearMonth::daysInMonth() const
    {
        return static_cast<int>(static_cast<unsigned>(lastDay().day()));
    }

    std::string YearMonth::toKey() const
    {
        return std::format("{:04}-{:02}", m_year, m_month);
    }

    std::string YearMonth::monthNameKey() const
    {
        return std::string(MONTH_NAME_KEYS[static_cast<std::size_t>(m_month - 1)]);
    }

    std::string YearMonth::monthName(const LocaleManager& localeManager) const
    {
        return localeManager.translate(monthNameKey());
    }

    std::string YearMonth::label(const LocaleManager& localeManager) const
    {
        return std::format("{} {}", monthName(localeManager), m_year);
    }

    YearMonth YearMonth::addMonths(const int monthCount) const
    {
        const long long target = static_cast<long long>(toMonthIndex(m_year, m_month)) + monthCount;
        const long long targetYear = target / MONTHS_PER_YEAR;
        const long long targetMonth = target % MONTHS_PER_YEAR + 1;

        if (target < 0 || !isValid(static_cast<int>(targetYear), static_cast<int>(targetMonth)))
            throw std::out_of_range("YearMonth arithmetic leaves the supported range");

        return YearMonth(static_cast<int>(targetYear), static_cast<int>(targetMonth));
    }

    YearMonth YearMonth::next() const
    {
        return addMonths(1);
    }

    YearMonth YearMonth::previous() const
    {
        return addMonths(-1);
    }

    int YearMonth::monthsUntil(const YearMonth& other) const
    {
        return toMonthIndex(other.m_year, other.m_month) - toMonthIndex(m_year, m_month);
    }

    std::chrono::year_month_day YearMonth::firstDay() const
    {
        return std::chrono::year_month_day{
            std::chrono::year{m_year},
            std::chrono::month{static_cast<unsigned>(m_month)},
            std::chrono::day{1}};
    }

    std::chrono::year_month_day YearMonth::lastDay() const
    {
        const std::chrono::year_month_day_last last{
            std::chrono::year{m_year},
            std::chrono::month_day_last{std::chrono::month{static_cast<unsigned>(m_month)}}};
        return std::chrono::year_month_day{last};
    }
}
