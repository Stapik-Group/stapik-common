# Date and month pickers

`stapik/ui/widget/DatePicker.hpp` and `MonthPicker.hpp` are menu buttons that open a popover with a `Gtk::Calendar`.
The button shows the current value: the date in the format of the current language (`formatDate`) or the month and
year (`YearMonth::label`). Both labels are refreshed when the language changes.

```cpp
DatePicker dueDate(std::chrono::year_month_day{2026y / October / 12d});
dueDate.signalDateChanged().connect([&dueDate] { filter.setDay(dueDate.date()); });

MonthPicker month(domain::YearMonth(2026, 10));
month.signalMonthChanged().connect([&month] { view.show(month.month()); });
```

The pickers take their initial value in the constructor, so the application decides what "today" is (for example
with its `IClock`). `DatePicker::setDate` throws `std::invalid_argument` for a date that does not exist.

## Behaviour

- `DatePicker` changes its date with every selection in the calendar and closes the popover when a day of the shown
  month is clicked. Moving to another month keeps the popover open; the day is kept, or cut to the last day of a
  shorter month, and becomes the new date.
- `MonthPicker` follows the month shown by the calendar, so moving between months changes `month()` at once. Clicking
  a day of the chosen month closes the popover.
- `setDate` and `setMonth` emit the signal only when the value really changes, and the calendar moves to the new value
  without emitting it a second time.

The names of months and weekdays inside the calendar come from GTK, so they follow the system language rather than
the language set in `LocaleManager`.
