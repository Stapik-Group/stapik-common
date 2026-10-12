#pragma once

#include "stapik/task/DebouncedAction.hpp"

#include <gtkmm/searchentry.h>
#include <sigc++/signal.h>

#include <chrono>
#include <string>

class SearchEntry : public Gtk::SearchEntry
{
public:
    static constexpr std::chrono::milliseconds DEFAULT_DELAY{300};

    explicit SearchEntry(std::chrono::milliseconds delay = DEFAULT_DELAY);

    SearchEntry(const SearchEntry&) = delete;
    SearchEntry& operator=(const SearchEntry&) = delete;

    void setDelay(std::chrono::milliseconds delay);
    [[nodiscard]] std::string query() const;

    sigc::signal<void()>& signalQueryChanged();

private:
    sigc::signal<void()> m_signalQueryChanged;
    stapik::task::DebouncedAction m_debounce;
};
