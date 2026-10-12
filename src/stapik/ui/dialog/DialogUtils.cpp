#include "DialogUtils.hpp"

#include "stapik/locale/LocaleManager.hpp"

#include <gtkmm/alertdialog.h>

void showMessageDialog(Gtk::Window& parent, const Glib::ustring& primaryText, const Glib::ustring& secondaryText)
{
    const auto dialog = Gtk::AlertDialog::create(primaryText);
    dialog->set_detail(secondaryText);
    dialog->set_modal(true);
    dialog->set_buttons({LocaleManager::instance().translate("dialog.button.ok")});
    dialog->set_default_button(0);
    dialog->set_cancel_button(0);
    dialog->show(parent);
}
