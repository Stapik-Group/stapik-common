#pragma once

#include "SettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <sigc++/scoped_connection.h>

#include <filesystem>
#include <string>

namespace stapik::ui
{
    enum class PathKind
    {
        File,
        Folder
    };

    class PathSettingRow final : public SettingRow
    {
    public:
        PathSettingRow(settings::ObservableSetting<std::filesystem::path>& setting, PathKind kind, std::string titleKey);

        [[nodiscard]] Gtk::Widget& widget() override;
        void retranslate() override;

    private:
        void commitEntry() const;
        void browse();

        settings::ObservableSetting<std::filesystem::path>& m_setting;
        PathKind m_kind;
        std::string m_titleKey;
        Gtk::Box m_box;
        Gtk::Entry m_entry;
        Gtk::Button m_browseButton;
        sigc::scoped_connection m_settingConnection;
    };
}
