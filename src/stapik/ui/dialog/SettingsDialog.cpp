#include "SettingsDialog.hpp"

#include "DialogUtils.hpp"

#include "stapik/locale/LocaleManager.hpp"

namespace
{
    constexpr int GRID_COLUMN_SPACING = 16;
    constexpr int GRID_ROW_SPACING = 12;
    constexpr int DIALOG_WIDTH = 480;
}

SettingsDialog::SettingsDialog(Window& parent, const stapik::ui::SettingsLayout& layout) :
    StapikDialog(parent, {})
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
        line->row->widget().set_hexpand(true);

        m_grid.attach(line->label, 0, gridRow);
        m_grid.attach(line->row->widget(), 1, gridRow);
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
