#include "CalendarButton.hpp"

#include "stapik/locale/LocaleManager.hpp"

#include <glibmm/datetime.h>

CalendarButton::CalendarButton()
{
    m_popover.set_child(m_calendar);
    set_popover(m_popover);

    m_calendar.signal_day_selected().connect([this]
    {
        if (!m_showingDate)
            onDaySelected();
    });

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { refreshLabel(); });
}

CalendarButton::~CalendarButton()
{
    m_localeConnection.disconnect();
}

void CalendarButton::showDate(const std::chrono::year_month_day& date)
{
    m_showingDate = true;
    m_calendar.select_day(Glib::DateTime::create_local(
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned>(date.month())),
        static_cast<int>(static_cast<unsigned>(date.day())),
        0, 0, 0));
    m_showingDate = false;
}

std::chrono::year_month_day CalendarButton::selectedDate() const
{
    const auto date = m_calendar.get_date();
    return std::chrono::year{date.get_year()}
           / std::chrono::month{static_cast<unsigned>(date.get_month())}
           / std::chrono::day{static_cast<unsigned>(date.get_day_of_month())};
}

void CalendarButton::closePopover()
{
    popdown();
}
