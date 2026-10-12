#include "DatePicker.hpp"

#include "stapik/locale/DateFormatter.hpp"
#include "stapik/locale/LocaleManager.hpp"

#include <stdexcept>

using stapik::domain::YearMonth;

DatePicker::DatePicker(const std::chrono::year_month_day& date)
{
    setDate(date);
}

void DatePicker::setDate(const std::chrono::year_month_day& date)
{
    if (!date.ok())
        throw std::invalid_argument("DatePicker needs a valid date");

    m_shownMonth = YearMonth::fromDate(date);
    showDate(date);

    if (m_date == date)
    {
        refreshLabel();
        return;
    }

    m_date = date;
    refreshLabel();
    m_signalDateChanged.emit();
}

const std::chrono::year_month_day& DatePicker::date() const
{
    return m_date;
}

sigc::signal<void()>& DatePicker::signalDateChanged()
{
    return m_signalDateChanged;
}

void DatePicker::onDaySelected()
{
    const auto selected = selectedDate();
    const auto selectedMonth = YearMonth::fromDate(selected);
    const bool onlyNavigated = selectedMonth != m_shownMonth;
    m_shownMonth = selectedMonth;

    if (selected != m_date)
    {
        m_date = selected;
        refreshLabel();
        m_signalDateChanged.emit();
    }

    if (!onlyNavigated)
        closePopover();
}

void DatePicker::refreshLabel()
{
    set_label(stapik::locale::formatDate(m_date, LocaleManager::instance().languageCode()));
}
