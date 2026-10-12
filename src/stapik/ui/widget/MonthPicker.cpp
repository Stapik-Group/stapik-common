#include "MonthPicker.hpp"

#include "stapik/locale/LocaleManager.hpp"

using stapik::domain::YearMonth;

MonthPicker::MonthPicker(const YearMonth& month) :
    m_month(month)
{
    showDate(m_month.firstDay());
    MonthPicker::refreshLabel();
}

void MonthPicker::setMonth(const YearMonth& month)
{
    showDate(month.firstDay());

    if (m_month == month)
        return;

    m_month = month;
    refreshLabel();
    m_signalMonthChanged.emit();
}

const YearMonth& MonthPicker::month() const
{
    return m_month;
}

sigc::signal<void()>& MonthPicker::signalMonthChanged()
{
    return m_signalMonthChanged;
}

void MonthPicker::onDaySelected()
{
    const auto selected = YearMonth::fromDate(selectedDate());

    if (selected == m_month)
    {
        closePopover();
        return;
    }

    m_month = selected;
    refreshLabel();
    m_signalMonthChanged.emit();
}

void MonthPicker::refreshLabel()
{
    set_label(m_month.label(LocaleManager::instance()));
}
