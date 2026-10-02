#include "AboutDialog.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/locale/LocaleManager.hpp"

#include <gtkmm/aboutdialog.h>

void showAboutDialog(Gtk::Window& parent, const stapik::app::AppInfo& appInfo)
{
    const auto name = appInfo.displayName.empty() ? appInfo.internalName : appInfo.displayName;

    auto* dialog = new Gtk::AboutDialog();
    dialog->set_transient_for(parent);
    dialog->set_modal(true);
    dialog->set_title(LocaleManager::instance().translate("dialog.about.title", { { "name", name } }));
    dialog->set_program_name(name);
    dialog->set_version(appInfo.version);

    if (!appInfo.author.empty())
        dialog->set_authors({ appInfo.author });

    if (!appInfo.repositoryUrl.empty())
        dialog->set_website(appInfo.repositoryUrl);

    dialog->signal_hide().connect([dialog] { delete dialog; });
    dialog->show();
}

void showAboutDialog(Gtk::Window& parent)
{
    showAboutDialog(parent, stapik::app::AppContext::instance().info());
}
