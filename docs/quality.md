# Sanitizers and code coverage

Four options instrument every Stapik target (the library, the tests and the applications built with
`stapik_add_application()`). They work with GCC and Clang; other compilers get a warning and build without them.

| Option                   | Effect                                                                    |
|--------------------------|---------------------------------------------------------------------------|
| `STAPIK_ENABLE_ASAN`     | AddressSanitizer: out-of-bounds access, use after free, leaks             |
| `STAPIK_ENABLE_UBSAN`    | UndefinedBehaviorSanitizer; a finding aborts the program, so a test fails |
| `STAPIK_ENABLE_TSAN`     | ThreadSanitizer: data races and lock-order inversions                     |
| `STAPIK_ENABLE_COVERAGE` | gcov instrumentation, built with `-O0 -g` so that lines map to the source |

`STAPIK_ENABLE_ASAN` and `STAPIK_ENABLE_TSAN` exclude each other; configuration fails when both are on. ASan and UBSan
can be combined.

## Running the tests

```bash
cmake -S . -B build-asan -DSTAPIK_COMMON_BUILD_TESTS=ON -DSTAPIK_ENABLE_ASAN=ON -DSTAPIK_ENABLE_UBSAN=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -DSTAPIK_COMMON_BUILD_TESTS=ON -DSTAPIK_ENABLE_TSAN=ON
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

ThreadSanitizer is the one to run for code with threads: `BackgroundTaskRunner`, `AsyncSyncScheduler` and the
`MainThread` hand-off. Use a separate build directory per sanitizer.

## In CI

The `sanitizers` and `coverage` jobs of `.github/workflows/ci.yml` run these configurations on every pull request from
`develop`; see [ci](ci.md). The tests do not use POSIX only calls, so the same suite also runs on Windows. Test code
creates directories with `stapik::test::createTemporaryDirectory()` and changes the environment with
`stapik::test::setEnvironment()`, both from `tests/support`.

## Coverage

```bash
cmake -S . -B build-coverage -DSTAPIK_COMMON_BUILD_TESTS=ON -DSTAPIK_ENABLE_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage
gcovr --root . --filter src --html-details build-coverage/coverage.html build-coverage
```

`gcovr` is not part of the build. The options are cache variables, so an application that pulls in stapik-common
with `FetchContent` turns them on for its own targets and for the library with the same `-D` flags; in CI pass them
through `cmake-args` (see [ci](ci.md)).
