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
- **Timestamps** – consistent time handling and formatting
- **Cloud storage** – a clean `ICloudStorage` interface plus a cloud client
- **Security helpers** – URL normalization and safe defaults (plain HTTP is allowed, but loudly logged as insecure)
- **GTK helpers** – shared gtkmm-4 pieces for a consistent look and feel

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

| What      | Version                    |
|-----------|----------------------------|
| Compiler  | C++20 capable (GCC / Clang)|
| CMake     | 3.x+                       |
| gtkmm     | 4.x                        |

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