#pragma once

#include "CalendarButton.hpp"

#include "stapik/domain/YearMonth.hpp"

#include <sigc++/signal.h>

class MonthPicker : public CalendarButton
{
public:
    explicit MonthPicker(const stapik::domain::YearMonth& month);

    void setMonth(const stapik::domain::YearMonth& month);
    [[nodiscard]] const stapik::domain::YearMonth& month() const;

    sigc::signal<void()>& signalMonthChanged();

protected:
    void onDaySelected() override;
    void refreshLabel() override;

private:
    stapik::domain::YearMonth m_month;
    sigc::signal<void()> m_signalMonthChanged;
};
