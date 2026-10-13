#pragma once

#include "StapikDialog.hpp"

#include "stapik/ui/settings/SettingRow.hpp"
#include "stapik/ui/settings/SettingsLayout.hpp"

#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/sizegroup.h>
#include <sigc++/scoped_connection.h>

#include <memory>
#include <string>
#include <vector>

class SettingsDialog final : public StapikDialog
{
public:
    SettingsDialog(Window& parent, const stapik::ui::SettingsLayout& layout);

private:
    struct Line
    {
        std::string labelKey;
        Gtk::Label label;
        std::unique_ptr<stapik::ui::SettingRow> row;
    };

    void retranslate();

    Gtk::Grid m_grid;
    Glib::RefPtr<Gtk::SizeGroup> m_controlSizes;
    std::vector<std::unique_ptr<Line>> m_lines;
    Gtk::Button* m_closeButton = nullptr;
    sigc::scoped_connection m_localeConnection;
};

void showSettingsDialog(Gtk::Window& parent, const stapik::ui::SettingsLayout& layout);
