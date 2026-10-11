# Command line

Every application built on `StapikApplication` understands these flags (see `--help`):

| Flag | Effect |
| --- | --- |
| `--version` | Prints `<name> <version>` and exits. |
| `--log-level=LEVEL` | `debug`, `info`, `warning` or `error`. Overrides `STAPIK_LOG_LEVEL`. |
| `--data-dir=DIR` | Keeps data, settings and cache in `DIR/{data,config,cache}`. Relative paths are resolved against the working directory. |
| `--portable` | Same as `--data-dir`, with `portable/` next to the executable. Cannot be combined with `--data-dir`. |
| `--reset-settings` | Moves `settings.json` (and the legacy `locale.txt`, `theme.txt`) aside as `*.bak` before the application starts. |
| `--safe-mode` | The window state is neither restored nor saved. Applications can read `commandLine().safeMode` to skip risky startup work. |

The flags are applied in `on_handle_local_options`, before settings are loaded. Application code must not touch
settings, `AppPaths` or `LocaleManager` before `run()`, otherwise `--data-dir`, `--portable` and
`--reset-settings` do not take effect.

On Windows the stderr log and the pixbuf loaders cache are set up earlier, in `AppContext::initialize()`, and still
go to `%LOCALAPPDATA%` in portable mode.
