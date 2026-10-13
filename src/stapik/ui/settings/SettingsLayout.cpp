#include "SettingsLayout.hpp"

namespace stapik::ui
{
    SettingsLayout& SettingsLayout::addBool(std::string labelKey, settings::ObservableSetting<bool>& setting)
    {
        return add(std::move(labelKey), [&setting] { return std::make_unique<BoolSettingRow>(setting); });
    }

    SettingsLayout& SettingsLayout::addFont(std::string labelKey, settings::ObservableSetting<std::string>& setting)
    {
        return add(std::move(labelKey), [&setting] { return std::make_unique<FontSettingRow>(setting); });
    }

    SettingsLayout& SettingsLayout::addPath(
        std::string labelKey,
        settings::ObservableSetting<std::filesystem::path>& setting,
        const PathKind kind)
    {
        return add(labelKey, [&setting, kind, labelKey]
        {
            return std::make_unique<PathSettingRow>(setting, kind, labelKey);
        });
    }

    const std::vector<SettingsLayout::Entry>& SettingsLayout::entries() const
    {
        return m_entries;
    }

    SettingsLayout& SettingsLayout::add(std::string labelKey, std::function<std::unique_ptr<SettingRow>()> createRow)
    {
        m_entries.push_back({ .labelKey = std::move(labelKey), .createRow = std::move(createRow) });
        return *this;
    }
}
