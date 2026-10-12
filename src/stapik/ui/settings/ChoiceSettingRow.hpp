#pragma once

#include "ChoiceDropDown.hpp"
#include "SettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <sigc++/scoped_connection.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace stapik::ui
{
    template<typename ValueType>
    struct ChoiceOption
    {
        ValueType value;
        std::string label;
    };

    template<typename ValueType>
    class ChoiceSettingRow final : public SettingRow
    {
    public:
        using Options = std::vector<ChoiceOption<ValueType>>;
        using OptionsProvider = std::function<Options()>;

        ChoiceSettingRow(settings::ObservableSetting<ValueType>& setting, OptionsProvider optionsProvider) :
            m_setting(setting),
            m_optionsProvider(std::move(optionsProvider))
        {
            loadOptions();
            m_dropDown.select(indexOf(m_setting.get()));

            m_dropDown.signalChosen().connect([this](const std::size_t index) { m_setting.set(m_options[index].value); });
            m_settingConnection = m_setting.signalChanged().connect([this](const ValueType& value)
            {
                m_dropDown.select(indexOf(value));
            });
        }

        [[nodiscard]] Gtk::Widget& widget() override
        {
            return m_dropDown;
        }

        void retranslate() override
        {
            loadOptions();
            m_dropDown.select(indexOf(m_setting.get()));
        }

    private:
        void loadOptions()
        {
            m_options = m_optionsProvider();

            std::vector<std::string> labels;
            for (const auto& option : m_options)
                labels.push_back(option.label);

            m_dropDown.setLabels(labels);
        }

        [[nodiscard]] std::optional<std::size_t> indexOf(const ValueType& value) const
        {
            const auto option = std::ranges::find(m_options, value, &ChoiceOption<ValueType>::value);
            if (option == m_options.end())
                return std::nullopt;

            return static_cast<std::size_t>(option - m_options.begin());
        }

        settings::ObservableSetting<ValueType>& m_setting;
        OptionsProvider m_optionsProvider;
        Options m_options;
        ChoiceDropDown m_dropDown;
        sigc::scoped_connection m_settingConnection;
    };
}
