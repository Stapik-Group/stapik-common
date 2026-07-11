#include "DialogUtils.hpp"

void showMessageDialog(Gtk::Window& parent,
                        const Glib::ustring& primaryText,
                        const Glib::ustring& secondaryText,
                        const Gtk::MessageType type)
{
    auto* dialog = showAutoDeletingDialog<Gtk::MessageDialog>(parent, primaryText, false, type, Gtk::ButtonsType::OK, true);
    dialog->set_secondary_text(secondaryText);
}