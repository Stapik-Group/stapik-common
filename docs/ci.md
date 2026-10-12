# Continuous integration

stapik-common ships GitHub Actions workflows that an application calls instead of writing its own. They cover Linux
(in Docker images, so that a `.deb` is built for every distribution that is supported) and Windows (MSYS2 UCRT64).

The rules every Stapik repository follows:

| Event                                            | What happens                                                                           |
|--------------------------------------------------|----------------------------------------------------------------------------------------|
| push to any branch                               | nothing                                                                                |
| pull request from `develop` to `main` / `master` | build and tests on Linux and Windows                                                   |
| merge into `main` / `master`                     | nothing                                                                                |
| release published (with its tag)                 | `.deb` for every image, Windows installer and portable ZIP are attached to the release |

The "nothing" cases are simply the absence of a trigger: an application does not add a `push` trigger.

## Using them

Copy the files from `cmake/templates/github/` into `.github/workflows/` of the application and adjust the marked
values. They call the reusable workflows of this repository by tag, so pin `@v...` to the stapik-common release you
use, like the `GIT_TAG` of the dependency.

### Pull request check: `pull-request.yml`

Calls `build.yml` for pull requests to `main` and `master` and skips them unless the branch is `develop`. The inputs
of `build.yml`:

| Input              | Default             | Meaning                                                                          |
|--------------------|---------------------|----------------------------------------------------------------------------------|
| `application-name` |                     | `NAME` of `stapik_add_application()`; needed for the Windows packages            |
| `images`           | `["ubuntu:24.04"]`  | JSON array of the Docker images of the Linux build                               |
| `cmake-args`       |                     | extra configuration arguments, for both systems; the place to turn the tests on  |
| `apt-packages`     |                     | extra apt packages of the application                                            |
| `msys2-packages`   |                     | extra MSYS2 packages in pacboy syntax (`libarchive:p`)                           |
| `run-tests`        | `true`              | run `ctest` (on Linux inside `xvfb-run`)                                         |
| `windows`          | `true`              | also build on Windows                                                            |
| `package`          | `false`             | build the packages and upload them as workflow artifacts (used by the release)   |

The Linux job installs gtkmm 4, libcurl, libsodium, zlib, libsecret and libx11 with apt, and CMake and Ninja with
pip, so the CMake version does not depend on the distribution. The Windows job installs the MSYS2 packages listed in
[windows](windows.md).
