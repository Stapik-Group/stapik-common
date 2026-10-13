#include "SettingsDialog.hpp"

#include "DialogUtils.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/ui/settings/ChoiceDropDown.hpp"

#include <gtkmm/spinbutton.h>
#include <gtkmm/switch.h>

namespace
{
    constexpr int GRID_COLUMN_SPACING = 24;
    constexpr int GRID_ROW_SPACING = 8;
    constexpr int DIALOG_WIDTH = 520;
    constexpr int ROW_MIN_HEIGHT = 40;
    constexpr int CONTROL_MIN_WIDTH = 168;

    bool isCompactControl(Gtk::Widget& widget)
    {
        return dynamic_cast<stapik::ui::ChoiceDropDown*>(&widget) != nullptr
            || dynamic_cast<Gtk::SpinButton*>(&widget) != nullptr;
    }
}

SettingsDialog::SettingsDialog(Window& parent, const stapik::ui::SettingsLayout& layout) :
    StapikDialog(parent, {}),
    m_controlSizes(Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::BOTH))
{
    set_default_size(DIALOG_WIDTH, -1);

    m_grid.set_column_spacing(GRID_COLUMN_SPACING);
    m_grid.set_row_spacing(GRID_ROW_SPACING);
    contentBox().append(m_grid);

    int gridRow = 0;
    for (const auto& [labelKey, createRow] : layout.entries())
    {
        auto line = std::make_unique<Line>();
        line->labelKey = labelKey;
        line->row = createRow();
        line->label.set_halign(Gtk::Align::START);
        line->label.set_valign(Gtk::Align::CENTER);
        line->label.set_hexpand(true);
        line->label.set_size_request(-1, ROW_MIN_HEIGHT);

        auto& control = line->row->widget();
        if (isCompactControl(control))
        {
            control.set_halign(Gtk::Align::END);
            control.set_valign(Gtk::Align::CENTER);
            control.set_size_request(CONTROL_MIN_WIDTH, -1);
            m_controlSizes->add_widget(control);
        }
        else if (dynamic_cast<Gtk::Switch*>(&control) != nullptr)
        {
            control.set_halign(Gtk::Align::END);
            control.set_valign(Gtk::Align::CENTER);
        }
        else
        {
            // Wide rows, such as a path with its entry and a browse button, take the room that is left.
            control.set_hexpand(true);
        }

        m_grid.attach(line->label, 0, gridRow);
        m_grid.attach(control, 1, gridRow);
        m_lines.push_back(std::move(line));
        ++gridRow;
    }

    m_closeButton = add_button({}, Gtk::ResponseType::CLOSE);
    set_default_response(Gtk::ResponseType::CLOSE);

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { retranslate(); });
    retranslate();
}

void SettingsDialog::retranslate()
{
    const auto& loc = LocaleManager::instance();

    set_title(loc.translate("dialog.settings.title"));
    m_closeButton->set_label(loc.translate("dialog.button.close"));

    for (const auto& line : m_lines)
    {
        line->label.set_text(loc.translate(line->labelKey));
        line->row->retranslate();
    }
}

void showSettingsDialog(Gtk::Window& parent, const stapik::ui::SettingsLayout& layout)
{
    showAutoDeletingDialog<SettingsDialog>(parent, layout);
}
