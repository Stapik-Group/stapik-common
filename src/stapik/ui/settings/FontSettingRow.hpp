#pragma once

#include "SettingRow.hpp"

#include "stapik/settings/ObservableSetting.hpp"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/fontdialogbutton.h>
#include <pangomm/fontdescription.h>
#include <sigc++/scoped_connection.h>

#include <string>

namespace stapik::ui
{
    // A button that opens the font chooser of GTK, and a button that goes back to the font of the theme.
    class FontSettingRow final : public SettingRow
    {
    public:
        explicit FontSettingRow(settings::ObservableSetting<std::string>& setting);

        [[nodiscard]] Gtk::Widget& widget() override;
        [[nodiscard]] SettingRowLayout layout() const override;
        void retranslate() override;

    private:
        void showSetting();
        void onFontPicked();
        [[nodiscard]] Pango::FontDescription themeFont();

        settings::ObservableSetting<std::string>& m_setting;
        Gtk::Box m_box;
        Gtk::FontDialogButton m_button;
        Gtk::Button m_resetButton;
        bool m_programmaticChange = false;
        sigc::scoped_connection m_settingConnection;
    };
}
