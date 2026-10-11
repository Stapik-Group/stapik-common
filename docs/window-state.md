# Window state

`StapikApplication` remembers the main window between runs, through the application's `SettingsStore`.
The state is written when the window is asked to close and restored when the window is added to the application.

## What is remembered

| Key                             | Meaning                                                                                              |
|---------------------------------|------------------------------------------------------------------------------------------------------|
| `window.width`, `window.height` | Size of the window while it was neither maximized nor fullscreen.                                    |
| `window.maximized`              | Whether the window was maximized. A maximized window is restored maximized, at its last normal size. |
| `window.x`, `window.y`          | Position of the window while it was neither maximized nor fullscreen.                                |

Values that are not numbers or are out of range are ignored, so a hand-edited settings file cannot produce
a window that is tiny or far away.

## Position

GTK 4 has no API for the position of a window, so it is read and set through the windowing system:

| Platform | Position                                                                       |
|----------|--------------------------------------------------------------------------------|
| X11      | remembered (needs the `x11` pkg-config package at build time)                  |
| Windows  | remembered                                                                     |
| Wayland  | not remembered, only the size: a client cannot choose where its window appears |

A saved position is skipped when it lies outside the screen, for example after a monitor was disconnected.

## Paned dividers

The position of a `Gtk::Paned` divider is stored in pixels under `window.panes.<name>`. The application opts in per paned,
once the widget exists:

```cpp
application->rememberPanedPosition(m_sidebarPaned, "sidebar");
```

The divider is restored at once and stored when the paned is unmapped, which includes closing the window. The names
have to be unique within the application. `stapik::ui::rememberPanedPosition()` does the same against any
`SettingsStore`, without the checks of `StapikApplication`.

## Options

Set `StapikApplicationOptions::rememberWindowState` to `false` to turn the feature off, dividers included. It is
also off in safe mode (`--safe-mode`).

Only the first window of the application is remembered by default. Override `StapikApplication::windowStateKey()`
to remember other windows too: it returns the settings key prefix for the window, or an empty string to skip it.
