#pragma once

#include "CalendarButton.hpp"

#include "stapik/domain/YearMonth.hpp"

#include <sigc++/signal.h>

#include <chrono>

class DatePicker : public CalendarButton
{
public:
    explicit DatePicker(const std::chrono::year_month_day& date);

    void setDate(const std::chrono::year_month_day& date);
    [[nodiscard]] const std::chrono::year_month_day& date() const;

    sigc::signal<void()>& signalDateChanged();

protected:
    void onDaySelected() override;
    void refreshLabel() override;

private:
    std::chrono::year_month_day m_date;
    stapik::domain::YearMonth m_shownMonth;
    sigc::signal<void()> m_signalDateChanged;
};
