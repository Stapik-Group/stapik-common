#pragma once

#include <chrono>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

class LocaleManager;

namespace stapik::domain
{
    inline constexpr int MIN_YEAR_MONTH_YEAR = 1;
    inline constexpr int MAX_YEAR_MONTH_YEAR = 9999;
    inline constexpr int MONTHS_PER_YEAR = 12;

    class YearMonth
    {
    public:
        YearMonth() = default;

        YearMonth(int year, int month);

        [[nodiscard]] static std::optional<YearMonth> tryCreate(int year, int month);
        [[nodiscard]] static std::optional<YearMonth> fromKey(std::string_view key);
        [[nodiscard]] static YearMonth fromDate(const std::chrono::year_month_day& date);
        [[nodiscard]] static YearMonth current();

        [[nodiscard]] int year() const;
        [[nodiscard]] int month() const;
        [[nodiscard]] int daysInMonth() const;

        // Stable storage form "YYYY-MM"; lexicographic order equals chronological order.
        [[nodiscard]] std::string toKey() const;

        [[nodiscard]] std::string monthNameKey() const;
        [[nodiscard]] std::string monthName(const LocaleManager& localeManager) const;
        [[nodiscard]] std::string label(const LocaleManager& localeManager) const;

        [[nodiscard]] YearMonth addMonths(int monthCount) const;
        [[nodiscard]] YearMonth next() const;
        [[nodiscard]] YearMonth previous() const;

        [[nodiscard]] int monthsUntil(const YearMonth& other) const;

        [[nodiscard]] std::chrono::year_month_day firstDay() const;
        [[nodiscard]] std::chrono::year_month_day lastDay() const;

        [[nodiscard]] std::strong_ordering operator<=>(const YearMonth& other) const = default;
    private:
        int m_year = MIN_YEAR_MONTH_YEAR;
        int m_month = 1;
    };
}
