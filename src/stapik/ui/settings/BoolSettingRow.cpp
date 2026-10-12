#include "BoolSettingRow.hpp"

namespace stapik::ui
{
    BoolSettingRow::BoolSettingRow(settings::ObservableSetting<bool>& setting) :
        m_setting(setting)
    {
        m_switch.set_active(m_setting.get());
        m_switch.set_halign(Gtk::Align::START);
        m_switch.set_valign(Gtk::Align::CENTER);

        m_switch.property_active().signal_changed().connect([this] { m_setting.set(m_switch.get_active()); });
        m_settingConnection = m_setting.signalChanged().connect([this](const bool value) { m_switch.set_active(value); });
    }

    Gtk::Widget& BoolSettingRow::widget()
    {
        return m_switch;
    }
}
