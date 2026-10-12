#pragma once

#include "SettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <sigc++/scoped_connection.h>

#include <concepts>

namespace stapik::ui
{
    template<typename ValueType>
    concept NumberSettingValue = (std::integral<ValueType> && !std::same_as<ValueType, bool>) || std::floating_point<ValueType>;

    template<NumberSettingValue ValueType>
    struct NumberRange
    {
        ValueType min;
        ValueType max;
        ValueType step = 1;
        unsigned digits = 0;
    };

    template<NumberSettingValue ValueType>
    class NumberSettingRow final : public SettingRow
    {
    public:
        NumberSettingRow(settings::ObservableSetting<ValueType>& setting, const NumberRange<ValueType>& range) :
            m_setting(setting),
            m_spin(Gtk::Adjustment::create(
                static_cast<double>(setting.get()),
                static_cast<double>(range.min),
                static_cast<double>(range.max),
                static_cast<double>(range.step),
                static_cast<double>(range.step) * PAGE_STEP_MULTIPLIER),
                0.0,
                range.digits)
        {
            m_spin.set_halign(Gtk::Align::START);
            m_spin.set_valign(Gtk::Align::CENTER);

            m_spin.signal_value_changed().connect([this] { m_setting.set(static_cast<ValueType>(m_spin.get_value())); });
            m_settingConnection = m_setting.signalChanged().connect([this](const ValueType& value)
            {
                m_spin.set_value(static_cast<double>(value));
            });
        }

        [[nodiscard]] Gtk::Widget& widget() override
        {
            return m_spin;
        }

    private:
        static constexpr double PAGE_STEP_MULTIPLIER = 10.0;

        settings::ObservableSetting<ValueType>& m_setting;
        Gtk::SpinButton m_spin;
        sigc::scoped_connection m_settingConnection;
    };
}
