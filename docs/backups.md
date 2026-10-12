# Document backups

`DocumentFile` copies the data file before every save. The copies are named `<file>.bak` (the newest), `<file>.bak.2`
and so on, up to the number given to `setBackupCount()`. The default is 1; 0 turns backups off. A file that is not
valid JSON is never copied, so a broken file cannot push a good backup out.

The date of a backup is the date its contents were saved, not the date of the copy.

## Restoring

`listBackups()` returns the backups that exist, newest first. `showRestoreBackupDialog()`
(`stapik/ui/dialog/RestoreBackupDialog.hpp`) lets the user pick one by its date and time, and calls back with it.
With no backups it shows a message instead. `loadBackup()` reads a backup the way `importFrom()` reads any file:
the managed file is not touched, a damaged backup is reported as `Corrupted` and stays where it is, and an older
schema version is migrated.

```cpp
showRestoreBackupDialog(*this, m_documentFile.listBackups(), [this](const stapik::document::BackupInfo& backup)
{
    const auto result = m_documentFile.loadBackup(backup);
    if (result.status != stapik::document::LoadStatus::Loaded)
    {
        showMessageDialog(*this, ...);
        return;
    }

    replaceDocument(*result.document);
    if (const auto saved = m_documentFile.save(*result.document); !saved)
        showMessageDialog(*this, ..., saved.error().message());
});
```

Saving the restored document makes the file as it was before the restore the newest backup, so a restore can be
taken back the same way.

The menu item belongs to the application. The shared label is `menu.file.restoreBackup`:

```cpp
standardMenu.addToMenu(StandardMenu::Target::File, [](Gio::Menu& menu)
{
    menu.append(LocaleManager::instance().translate("menu.file.restoreBackup"), "win.restoreBackup");
});
```
