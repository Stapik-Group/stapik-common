#include "UpdateDialog.hpp"

#include "ConfirmDialog.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"

#include <giomm/appinfo.h>
#include <glibmm/error.h>

#include <string>

namespace
{
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
}

void showUpdateAvailableDialog(Gtk::Window& parent, const stapik::app::AppInfo& appInfo, const stapik::update::ReleaseInfo& release)
{
    const auto& loc = LocaleManager::instance();
    const auto& name = appInfo.displayName.empty() ? appInfo.internalName : appInfo.displayName;

    const ConfirmDialogOptions options{
        .title = loc.translate("dialog.update.title"),
        .message = loc.translate("dialog.update.message", {
            { "name", name },
            { "latest", release.version.toString() },
            { "current", appInfo.version } }),
        .confirmLabel = loc.translate("dialog.update.download")
    };

    showConfirmDialog(parent, options, [url = release.pageUrl] { openInBrowser(url); });
}
