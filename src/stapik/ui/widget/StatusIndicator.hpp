#pragma once

#include "stapik/sync/SyncStatus.hpp"

#include <gtkmm/label.h>
#include <sigc++/connection.h>

class StatusIndicator : public Gtk::Label
{
public:
    StatusIndicator();
    ~StatusIndicator() override;

    StatusIndicator(const StatusIndicator&) = delete;
    StatusIndicator& operator=(const StatusIndicator&) = delete;

    void setStatus(stapik::sync::SyncStatus status);
    [[nodiscard]] stapik::sync::SyncStatus status() const;

private:
    void refresh();

    stapik::sync::SyncStatus m_status = stapik::sync::SyncStatus::Idle;
    sigc::connection m_localeConnection;
};
