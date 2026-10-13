#pragma once

#include "BoolSettingRow.hpp"
#include "ChoiceSettingRow.hpp"
#include "FontSettingRow.hpp"
#include "NumberSettingRow.hpp"
#include "PathSettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace stapik::ui
{
    class SettingsLayout
    {
    public:
        struct Entry
        {
            std::string labelKey;
            std::function<std::unique_ptr<SettingRow>()> createRow;
        };

        SettingsLayout& addBool(std::string labelKey, settings::ObservableSetting<bool>& setting);
        // The setting holds the text of a stapik::theme::FontChoice ("Inter 11"); empty means the font of the theme.
        SettingsLayout& addFont(std::string labelKey, settings::ObservableSetting<std::string>& setting);
        SettingsLayout& addPath(std::string labelKey, settings::ObservableSetting<std::filesystem::path>& setting, PathKind kind);

        template<typename ValueType>
        SettingsLayout& addChoice(
            std::string labelKey,
            settings::ObservableSetting<ValueType>& setting,
            ChoiceSettingRow<ValueType>::OptionsProvider options)
        {
            return add(std::move(labelKey), [&setting, options = std::move(options)]
            {
                return std::make_unique<ChoiceSettingRow<ValueType>>(setting, options);
            });
        }

        template<typename ValueType>
        SettingsLayout& addChoice(
            std::string labelKey,
            settings::ObservableSetting<ValueType>& setting,
            ChoiceSettingRow<ValueType>::Options options)
        {
            return addChoice<ValueType>(std::move(labelKey), setting, [options = std::move(options)] { return options; });
        }

        template<NumberSettingValue ValueType>
        SettingsLayout& addNumber(
            std::string labelKey,
            settings::ObservableSetting<ValueType>& setting,
            const std::type_identity_t<NumberRange<ValueType>>& range)
        {
            return add(std::move(labelKey), [&setting, range]
            {
                return std::make_unique<NumberSettingRow<ValueType>>(setting, range);
            });
        }

        [[nodiscard]] const std::vector<Entry>& entries() const;

    private:
        SettingsLayout& add(std::string labelKey, std::function<std::unique_ptr<SettingRow>()> createRow);

        std::vector<Entry> m_entries;
    };
}
