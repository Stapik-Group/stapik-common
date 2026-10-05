# Windows

stapik-common builds on Windows with MSYS2 (UCRT64 or MINGW64, GCC, gtkmm-4.0). Applications are distributed as
a self-contained folder; there is no installer.

## What differs from Linux

| Area | Linux | Windows |
| --- | --- | --- |
| Data directory | `$XDG_DATA_HOME/<app>` | `%APPDATA%\<app>\data` |
| Config directory | `$XDG_CONFIG_HOME/<app>` | `%APPDATA%\<app>\config` |
| Cache directory | `$XDG_CACHE_HOME/<app>` | `%LOCALAPPDATA%\<app>\cache` |
| Executable directory | `/proc/self/exe` | `GetModuleFileNameW` |
| Atomic file write | temp file, `fsync`, `rename`, directory `fsync` | temp file, `FlushFileBuffers`, `MoveFileExW(REPLACE_EXISTING \| WRITE_THROUGH)` with a short retry when a virus scanner holds the file |
| Console output | stderr | stderr is written to `%LOCALAPPDATA%\<app>\cache\stapik.log` (GUI programs have no console) |
| Single instance | GApplication | named mutex (`SingleInstanceGuard`) |
| TLS certificates (cloud sync) | system | Windows certificate store, plus `etc/ssl/certs/ca-bundle.crt` next to the executable |

The `XDG_*_HOME` variables are honoured on Windows too (tests and portable setups use them).
File permissions are not applied on Windows: files inherit the access rules of the user profile.

Paths reach logs and GLib through `stapik::storage::pathText()`, which gives UTF-8. `path::string()` must not be
used for that: on Windows it converts to the ANSI code page and breaks on user names with characters outside it.

## Runtime of a bundled application

`AppContext::initialize()` calls `prepareWindowsRuntime()` first thing, which

* redirects stderr to the log file,
* points GLib and GTK at the data in the application folder (`GSETTINGS_SCHEMA_DIR`, `XDG_DATA_DIRS`,
  `FONTCONFIG_PATH`) and writes a copy of the gdk-pixbuf `loaders.cache` with the loader paths of that folder
  (`GDK_PIXBUF_MODULE_FILE`),
* selects the Cairo renderer unless `GSK_RENDERER` is set. GTK 4 would otherwise use OpenGL, which fails or shows a
  blank window on virtual machines and old graphics drivers; a list and forms do not need a GPU.

Applications should also acquire a `SingleInstanceGuard` right after `AppContext::initialize()`.

## Building the folder

`stapik_add_application()` produces a GUI executable with version information (and the icon, when the packaging
directory has `<name>.ico`). Configure with `-DSTAPIK_WINDOWS_CONSOLE=ON` for a build that keeps a console window.

In an MSYS2 shell:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target <name>_windows_bundle

creates `build/<name>-windows/`, which runs on a PC without MSYS2. The script is `cmake/templates/windows/bundle.sh`;
it fails when a DLL would be resolved from outside the folder.

Required MSYS2 packages: `gcc cmake ninja pkgconf gtkmm-4.0 curl adwaita-icon-theme librsvg` (prefixed
`mingw-w64-ucrt-x86_64-`).
