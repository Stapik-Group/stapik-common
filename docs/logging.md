# Logging

Messages go through `stapik::log` to GLib, and GLib writes them to stderr. The minimum level comes from
`STAPIK_LOG_LEVEL` or `--log-level`.

## Log file

`AppContext::initialize()` appends stderr to `stapik.log` in the cache directory of the application
(`$XDG_CACHE_HOME/<app>` on Linux, `%LOCALAPPDATA%\<app>\cache` on Windows). Each start adds a `---- started ... UTC ----`
line.

* Windows: always.
* Linux: only when stderr is not a terminal (a launcher, a service), so output stays visible when running from a shell.
* `STAPIK_LOG_FILE=off` disables the file on both platforms.

The file is rotated at start, once it has reached 1 MiB: `stapik.log` becomes `stapik.log.1`, `stapik.log.1` becomes
`stapik.log.2`, up to `stapik.log.4`, and the oldest is deleted. That keeps at most 5 MiB per application. The size is
not checked while the application runs.
