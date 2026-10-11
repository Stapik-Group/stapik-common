# Logging

`stapik::log::debug/info/warning/error` format a message with `std::format` and hand a `LogRecord` (level, text, UTC
time) to every registered `ILogSink`. The minimum level comes from `STAPIK_LOG_LEVEL` or `--log-level`; records below it
are dropped before they reach a sink.

## Sinks

| Sink             | Destination                                                                        |
|------------------|------------------------------------------------------------------------------------|
| `ConsoleSink`    | stderr, one `2026-10-11 12:00:00 [WARN] text` line per record                      |
| `FileSink`       | a file, rotated while the application runs                                         |
| `RingBufferSink` | the newest N records in memory; `snapshot()` returns them for a diagnostics dialog |

`log::sinks()`, `log::addSink()` and `log::setSinks()` manage the list, which starts with a single `ConsoleSink`.
The logger calls sinks under its own lock, so a sink needs no locking for `write()` and must not log itself. To add
a destination, implement `ILogSink::write()`.

## In-memory log

`AppContext::initialize()` also registers one `RingBufferSink` with the newest 2000 records (`log::memoryLog()`),
independently of `STAPIK_LOG_FILE`. Only records at or above the minimum level reach it, so `debug` lines appear
only with `--log-level debug`. `log::filterByLevel()`, `log::formatLogText()` and `log::formatBugReport()` turn its
snapshot into text; the last one adds the application, version and platform for a bug report.

## Log file

`AppContext::initialize()` adds a `FileSink` for `stapik.log` in the cache directory of the application
(`$XDG_CACHE_HOME/<app>` on Linux, `%LOCALAPPDATA%\<app>\cache` on Windows) and starts forwarding GLib messages
(`g_warning`, GTK and other libraries) to the sinks. Each start adds a `---- started ... UTC ----` line.

* Windows: always.
* Linux: only when stderr is not a terminal (a launcher, a service), so output stays visible when running from a shell.
* `STAPIK_LOG_FILE=off` disables the file on both platforms.

When `stapik.log` reaches 1 MiB, it becomes `stapik.log.1`, `stapik.log.1` becomes `stapik.log.2`, up to
`stapik.log.4`, and the oldest is deleted. That keeps at most 5 MiB per application. The check is made at start and
before every write.

Raw writes to stderr by native libraries (not through GLib) are no longer captured in the file.
