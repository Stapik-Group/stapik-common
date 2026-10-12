#pragma once

#include <gtkmm/calendar.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>
#include <sigc++/connection.h>

#include <chrono>

class CalendarButton : public Gtk::MenuButton
{
public:
    ~CalendarButton() override;

    CalendarButton(const CalendarButton&) = delete;
    CalendarButton& operator=(const CalendarButton&) = delete;

protected:
    CalendarButton();

    void showDate(const std::chrono::year_month_day& date);
    [[nodiscard]] std::chrono::year_month_day selectedDate() const;
    void closePopover();

    virtual void onDaySelected() = 0;
    virtual void refreshLabel() = 0;

private:
    Gtk::Popover m_popover;
    Gtk::Calendar m_calendar;
    sigc::connection m_localeConnection;
    bool m_showingDate = false;
};
