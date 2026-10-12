#include "SearchEntry.hpp"

#include "stapik/text/Trim.hpp"

SearchEntry::SearchEntry(const std::chrono::milliseconds delay) :
    m_debounce(delay, [this] { m_signalQueryChanged.emit(); })
{
    signal_changed().connect([this] { m_debounce.trigger(); });
    signal_activate().connect([this] { m_debounce.flush(); });
    signal_stop_search().connect([this]
    {
        set_text("");
        m_debounce.flush();
    });
}

void SearchEntry::setDelay(const std::chrono::milliseconds delay)
{
    m_debounce.setDelay(delay);
}

std::string SearchEntry::query() const
{
    return stapik::text::trim(get_text().raw());
}

sigc::signal<void()>& SearchEntry::signalQueryChanged()
{
    return m_signalQueryChanged;
}
