#include "DialogUtils.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"

#include <giomm/appinfo.h>
#include <glibmm/error.h>
#include <gtkmm/alertdialog.h>

void openInBrowser(const std::string& url)
{
    try
    {
        Gio::AppInfo::launch_default_for_uri(url);
    }
    catch (const Glib::Error& error)
    {
        stapik::log::warning("Cannot open {}: {}", url, error.what());
    }
}

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
