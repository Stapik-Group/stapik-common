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

### Release: `release.yml`

Runs when a release is published, which creates its tag as well. `build` calls `build.yml` with `package: true`:

* one `.deb` per image of `images`, built by `cpack` inside that image, so that the dependencies found by
  `dpkg-shlibdeps` match the distribution. The image is appended to the file name:
  `my-app-1.2.0-Linux-ubuntu-24.04.deb`,
* `<name>-<version>-setup.exe` (Inno Setup installer) and `<name>-<version>-windows-portable.zip` on Windows, see
  [windows](windows.md). Both are signed when the repository has the secrets `WINDOWS_SIGN_PFX_BASE64` (the `.pfx`
  file in Base64) and `WINDOWS_SIGN_PFX_PASSWORD`; without them the files stay unsigned.

`publish` then calls `publish.yml`, which collects the artifacts of all packages and attaches them to the release
(`gh release upload --clobber`, so a rerun replaces the files). Only that job has write access to the repository;
the builds run with read access.

The tests run before packaging; a failing test stops the release assets from being published. The version of the
packages is the `VERSION` in the application's `CMakeLists.txt`, so bump it before the tag is created.
