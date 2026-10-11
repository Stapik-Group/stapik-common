#include "RestoreBackupDialog.hpp"

#include "ChoiceDialog.hpp"
#include "DialogUtils.hpp"

#include "stapik/locale/DateFormatter.hpp"
#include "stapik/locale/LocaleManager.hpp"

#include <cstddef>
#include <utility>

void showRestoreBackupDialog(
    Gtk::Window& parent,
    std::vector<stapik::document::BackupInfo> backups,
    std::function<void(const stapik::document::BackupInfo&)> onRestore)
{
    const auto& loc = LocaleManager::instance();
    const auto title = loc.translate("dialog.restore.title");

    if (backups.empty())
    {
        showMessageDialog(parent, title, loc.translate("dialog.restore.none"), Gtk::MessageType::INFO);
        return;
    }

    ChoiceDialogOptions options{
        .title = title,
        .message = loc.translate("dialog.restore.message"),
        .confirmLabel = loc.translate("dialog.restore.button")
    };

    for (const auto& backup : backups)
    {
        const auto savedAt = stapik::locale::toLocalTime(backup.savedAt);
        options.choices.emplace_back(stapik::locale::formatDateTime(savedAt, loc.languageCode()));
    }

    showChoiceDialog(parent, options, [backups = std::move(backups), onRestore = std::move(onRestore)](const std::size_t chosenIndex)
    {
        if (chosenIndex < backups.size() && onRestore)
            onRestore(backups[chosenIndex]);
    });
}
