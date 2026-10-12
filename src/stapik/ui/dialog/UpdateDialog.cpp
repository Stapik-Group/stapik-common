#include "UpdateDialog.hpp"

#include "ConfirmDialog.hpp"
#include "DialogUtils.hpp"

#include "stapik/locale/LocaleManager.hpp"

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
