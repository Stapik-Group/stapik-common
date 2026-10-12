# File dialogs and drag & drop

## Dialogs

`stapik/ui/dialog/FileDialogs.hpp` wraps `Gtk::FileDialog` in three calls: `showOpenFileDialog`, `showSaveFileDialog`
and `showSelectFolderDialog`. A dismissed dialog calls nothing; a failure calls `onFailed` with the reason.

```cpp
const FileDialogOptions options{
    .title = LocaleManager::instance().translate("export.title"),
    .initialName = "report.csv",
    .lastDirectoryKey = "export.lastDirectory"};

showSaveFileDialog(window, options, [](const std::filesystem::path& path) { /* write it */ });
```

## Last directory

With `lastDirectoryKey` set, the dialog opens in the directory of the previous choice and stores the new one in the
application's `SettingsStore` (`AppContext`). Dialogs that should share a directory use the same key. The memory is
skipped when the key is empty, when `AppContext` is not initialized, and for a directory that no longer exists.

`loadLastDirectory()` and `saveLastDirectory()` in `stapik/ui/dialog/LastDirectory.hpp` do the same against any
`SettingsStore`. A chosen file stores its parent directory, a chosen folder stores itself.

## Drag & drop

`stapik/ui/dialog/FileDrop.hpp` makes any widget accept files dragged from a file manager:

```cpp
stapik::ui::acceptFileDrop(
    window,
    [](const std::vector<std::filesystem::path>& files) { /* import them */ },
    { .extensions = { ".csv" }, .allowMultiple = false });
```

| Option          | Default | Meaning                                                                                    |
|-----------------|---------|--------------------------------------------------------------------------------------------|
| `extensions`    | empty   | Accepted extensions with a leading dot, compared without regard to case. Empty takes all.  |
| `allowMultiple` | `true`  | When `false`, only the first accepted file is passed on.                                   |

A drop with no acceptable file is rejected and the handler is not called. `filterDroppedFiles()` is the same filter
without the widget.
