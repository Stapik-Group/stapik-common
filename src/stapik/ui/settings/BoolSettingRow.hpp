#pragma once

#include "SettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <gtkmm/switch.h>
#include <sigc++/scoped_connection.h>

namespace stapik::ui
{
    class BoolSettingRow final : public SettingRow
    {
    public:
        explicit BoolSettingRow(settings::ObservableSetting<bool>& setting);

        [[nodiscard]] Gtk::Widget& widget() override;

    private:
        settings::ObservableSetting<bool>& m_setting;
        Gtk::Switch m_switch;
        sigc::scoped_connection m_settingConnection;
    };
}
