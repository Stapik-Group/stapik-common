# Application archive

One ZIP file with everything an application keeps: the document, the settings and the files in the cloud (assets).
The user sees it as a single operation, "Back up the whole application…" and its counterpart "Restore the whole
application…". Shared labels: `menu.file.exportArchive`, `menu.file.importArchive`, and `dialog.archive.import.*`
for the confirmation before a restore. The menu items belong to the application, as with `menu.file.restoreBackup`
(see [backups.md](backups.md)).

The archive holds:

| Entry                 | Content                                                                    |
|-----------------------|----------------------------------------------------------------------------|
| `manifest.json`       | format and application name and version, creation date, list of the assets |
| `document.json`       | the text of the document file (with its schema version)                    |
| `settings.json`       | the text of the settings file; absent when the application has none        |
| `assets/<file name>`  | one entry per asset of the cloud storage                                   |

The cloud connection (address and API key) is not archived: an archive can be passed around without leaking it, and
a restore on another machine keeps that machine's connection.

## Backing up

```cpp
const stapik::archive::AppArchiveContext context{
    .appName = "budgeting",
    .appVersion = APP_VERSION,
    .settings = &settings,
    .assets = cloudAssets.get()   // nullptr when the application is not connected
};

const auto result = stapik::archive::exportAppArchive(m_documentFile, document, context, target);
if (!result)
    showMessageDialog(*this, ..., result.error().message(), Gtk::MessageType::ERROR);
```

The document is written from memory, so unsaved changes are included. The assets are downloaded one by one, which
takes as long as the network needs: run the call on a `BackgroundTaskRunner`. The archive appears under its final
name only when it is complete. An asset that vanishes during the backup, a file name that cannot be stored (`/`, `\`,
control characters) and an unreachable cloud fail the backup instead of leaving something out.

## Restoring

```cpp
const auto result = stapik::archive::importAppArchive(m_documentFile, source, context);
if (!result)
{
    showMessageDialog(*this, ..., result.error().message(), Gtk::MessageType::ERROR);
    return;
}

replaceDocument(result->document);
m_themeSetting.reload();
if (result->summary.assetsNotRestored > 0)
    ...   // the archive has files, but there is no cloud connection to put them in
```

The whole archive is read and checked first (manifest, application name, size and SHA-256 of every asset, CRC of
every entry, the document and the settings). A damaged archive, one of another application (`WrongApplication`) or one
made by a newer version (`Unsupported`) is refused before anything is changed. Then, in this order:

1. assets are uploaded; those already in the cloud with the same content are skipped, and files that exist only in
   the cloud are left alone, so a restore never deletes anything,
2. the document is saved to its file. The previous file becomes a backup, so the restore can be taken back,
3. the settings are replaced and saved. The returned `summary.settingsRestored` tells whether that worked; the
   document is already restored at that point, so it is not reported as an error.

A failure in step 1 (network, a file the server refuses as too big) leaves the document and the settings as they
were; the assets uploaded by then stay. Running the restore again continues where it stopped.

The returned document is already saved; the application only shows it. `ObservableSetting` does not notice that the
store was replaced: call `reload()` on each of them.

## Lower level

`writeAppArchive()` and `AppArchive` do the same without the document type: they carry the text of the document file
(`DocumentFile::serialize()` and `deserialize()`). `ZipWriter` and `ZipReader` (`stapik/archive/`) are plain ZIP
files, so a user can open an archive with any tool. Entries are deflated when that makes them smaller, otherwise
stored. ZIP64 (more than 4 GiB or 65534 files) and encrypted archives are not supported.
