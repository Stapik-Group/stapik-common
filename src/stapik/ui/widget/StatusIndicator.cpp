#include "StatusIndicator.hpp"

#include "stapik/locale/LocaleManager.hpp"

StatusIndicator::StatusIndicator()
{
    add_css_class("stapik-sync-status");
    set_halign(Gtk::Align::START);

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { refresh(); });

    refresh();
}

StatusIndicator::~StatusIndicator()
{
    m_localeConnection.disconnect();
}

void StatusIndicator::setStatus(const stapik::sync::SyncStatus status)
{
    m_status = status;
    refresh();
}

stapik::sync::SyncStatus StatusIndicator::status() const
{
    return m_status;
}

void StatusIndicator::refresh()
{
    for (const auto status : stapik::sync::ALL_SYNC_STATUSES)
        remove_css_class(stapik::sync::syncStatusCssClass(status));

    set_text(LocaleManager::instance().translate(stapik::sync::syncStatusKey(m_status)));
    add_css_class(stapik::sync::syncStatusCssClass(m_status));
}
