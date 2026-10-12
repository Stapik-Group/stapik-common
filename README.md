<div align="center">

# stapik-common

**The shared foundation behind every Stapik app.**
Write it once, use it everywhere.

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)
![gtkmm](https://img.shields.io/badge/gtkmm-4-4A86CF)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)
![Tests](https://img.shields.io/badge/tests-GoogleTest-34A853)

</div>

---

## What is this?

`stapik-common` is the common codebase for the Stapik desktop apps (Budgeting, Groceries and friends).
Instead of copy-pasting the same plumbing from project to project, every app pulls it in as a single CMake dependency and gets the same battle-tested building blocks:

- **Logging** – one logger, one format, across all apps
- **Safe file I/O** – atomic writes, so a crash never leaves a half-written data file
- **Results** – `Result<T, E>` and `FileError` say why a save failed instead of a bare `bool` (see [docs/results.md](docs/results.md))
- **Document backups** – rotating copies of the data file and a dialog to restore one of them (see [docs/backups.md](docs/backups.md))
- **Application archive** – one ZIP with the document, the settings and the cloud files: "back up the whole application" and restore it (see [docs/archive.md](docs/archive.md))
- **Timestamps and clock** – consistent time handling and formatting, and an injectable clock so tests do not depend on the system time (see [docs/clock.md](docs/clock.md))
- **Cloud storage** – a clean `ICloudStorage` interface plus a cloud client
- **HTTP client** – `IHttpClient` over libcurl, so network code is testable with a fake (see [docs/http.md](docs/http.md))
- **End-to-end encryption** – documents are encrypted before they leave the device; the cloud only stores a blob (see [docs/encryption.md](docs/encryption.md))
- **Compression** – large cloud documents are gzipped before they are sent (see [docs/compression.md](docs/compression.md))
- **Security helpers** – URL normalization and safe defaults (plain HTTP is allowed, but loudly logged as insecure)
- **Secret storage** – the cloud API key lives in the system keyring (Secret Service on Linux, Credential Manager on Windows), not in a plain file
- **Update checker** – a "new version available" notice based on GitHub Releases (see [docs/updates.md](docs/updates.md))
- **GTK helpers** – shared gtkmm-4 pieces for a consistent look and feel
- **Localization** – JSON translations with `{arg}` placeholders and CLDR plural forms (see [docs/localization.md](docs/localization.md))
- **Text search and sorting** – matching without case and diacritics (`ł` = `l`) and Polish-aware ordering (see [docs/text.md](docs/text.md))
- **CSV export** – RFC 4180 files, optionally with a byte order mark and a `;` separator for Polish Excel (see [docs/csv.md](docs/csv.md))
- **Money input** – `MoneyEntry` reads amounts with the separators of the current language and marks wrong input (see [docs/money-entry.md](docs/money-entry.md))
- **Date and month pickers** – `DatePicker` and `MonthPicker` open a calendar popover and report `year_month_day` / `YearMonth` (see [docs/date-pickers.md](docs/date-pickers.md))
- **File dialogs and drag & drop** – open/save/folder dialogs that remember the last directory, and a drop target that accepts files by extension (see [docs/file-dialogs.md](docs/file-dialogs.md))
- **Window state** – size, maximization, position and paned dividers are remembered between runs (see [docs/window-state.md](docs/window-state.md))
- **Settings window** – a dialog built declaratively from `ObservableSetting` rows: switches, choices, numbers and paths (see [docs/settings-window.md](docs/settings-window.md))

> The goal: apps contain only what makes them unique. Everything boring and repeatable lives here.

## Quick start

Add the library to your project with `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    stapik-common
    GIT_REPOSITORY https://github.com/Stapik-Group/stapik-common.git
    GIT_TAG        v1.1.6
)
FetchContent_MakeAvailable(stapik-common)

target_link_libraries(my_app PRIVATE stapik-common)
```

Then include what you need:

```cpp
#include <stapik/...>
```

Pin `GIT_TAG` to a release tag – never to a moving branch – so your app builds are reproducible.

## Requirements

| What      | Version                     |
|-----------|-----------------------------|
| Compiler  | C++20 capable (GCC / Clang) |
| CMake     | 3.x+                        |
| gtkmm     | 4.x                         |
| libsodium | 1.0.18+                     |
| libsecret | 1.x (Linux, optional)       |

## Building & testing

Tests are opt-in and use GoogleTest:

```bash
cmake -S . -B build -DSTAPIK_COMMON_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Workflow & releases

```
features/xyz  ──►  develop  ──►  main  ──►  tag vX.Y.Z
```

- New work lives on `features/*` branches
- `develop` is the integration branch
- `main` receives pull requests only; a release is a tag like `v1.2.0`
- CI runs on every branch except `develop`, on PRs to `main`, and builds the release artifact on `v*` tags

## Repository layout

```
stapik-common/
├── src/stapik/       library sources
├── CMakeLists.txt    build definition
└── README.md
```

## Contributing

1. Branch off `develop`: `git switch -c features/my-change`
2. Keep changes small and focused – one logical change per commit
3. Add or update tests for anything you touch
4. Open a pull request and make sure CI is green

---

<div align="center">
Maintained by <a href="https://github.com/Stapik-Group">Stapik Group</a>
</div>