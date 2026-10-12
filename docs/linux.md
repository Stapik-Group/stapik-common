# Linux packaging

`stapik_add_application()` installs a launcher, a desktop entry and an icon next to the application, and with
`cpack` builds a DEB package.

## Desktop entry

The entry is generated from `cmake/templates/application.desktop.in` (an application can provide its own
`application.desktop.in` in its packaging directory). Besides the name, comment and categories it can carry:

| Argument                 | Desktop entry key                  | Example                       |
|--------------------------|------------------------------------|-------------------------------|
| `LOCALIZED_NAMES`        | `Name[<language>]`                 | `pl=Budżet de=Haushalt`       |
| `LOCALIZED_DESCRIPTIONS` | `Comment[<language>]`              | `pl=Domowy budżet`            |
| `KEYWORDS`               | `Keywords`                         | `budget finance`              |
| `LOCALIZED_KEYWORDS`     | `Keywords[<language>]`, comma list | `pl=budżet,finanse`           |
| `MIME_TYPES`             | `MimeType`; `Exec` gets `%F`       | `application/x-stapik-budget` |
| `WM_CLASS`               | `StartupWMClass`                   | defaults to `APPLICATION_ID`  |

`StartupWMClass` has to match the application id for the shell to group the windows under the launcher. A MIME type
only makes the application show up in "Open with"; the type itself has to be registered by the application.

## AppStream metainfo

With `LICENSE` (an SPDX expression, e.g. `GPL-3.0-or-later`) the application also gets
`<APPLICATION_ID>.metainfo.xml` in `share/metainfo`, generated from `cmake/templates/application.metainfo.xml.in`
(overridable with `application.metainfo.xml.in` in the packaging directory). Software centers and Flathub read it.
It is filled from what `stapik_add_application()` already knows:

| Metainfo                | Source                                                                                          |
|-------------------------|-------------------------------------------------------------------------------------------------|
| `name`, `summary`       | `DISPLAY_NAME`, `DESCRIPTION`; translations from `LOCALIZED_NAMES` and `LOCALIZED_DESCRIPTIONS` |
| `description`           | `LONG_DESCRIPTION` (the description when it is missing)                                         |
| `developer`, `url`      | `MAINTAINER` (the id is `APPLICATION_ID` without its last part), `HOMEPAGE`                     |
| `keywords`, `mediatype` | `KEYWORDS`, `MIME_TYPES`                                                                        |
| `release`               | `VERSION` and `RELEASE_DATE` (`YYYY-MM-DD`; the day of the configuration when it is missing)    |

Set `RELEASE_DATE` for a release build so that the file does not change with every configuration. Flathub also wants
screenshots, which need a published URL: provide your own template for them. Check the result with
`appstreamcli validate --no-net <file>`.

## Flatpak

    cmake --build build --target <name>_flatpak

generates `<name>-flatpak/<application id>.yml` in the build directory and builds `<name>-<version>.flatpak` there
with `flatpak-builder` (the `org.gnome.Platform` runtime and SDK are installed from Flathub on the first run; the
runtime version is `STAPIK_FLATPAK_RUNTIME_VERSION`, 50 by default). The manifest

* builds gtkmm 4, libsodium and nlohmann_json from `cmake/templates/flatpak/stapik-dependencies.yml`, because the
  GNOME runtime does not contain them. The versions are git tags in that file,
* builds stapik-common from the sources the application uses and installs it as a CMake package into `/app`, then
  builds the application with `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=ALWAYS`, so that nothing is downloaded during the
  (offline) build: the application has to declare stapik-common with `FIND_PACKAGE_ARGS`,
* gives the sandbox network access (cloud sync), Wayland and X11 and the GPU. Files are reached through the file
  chooser portal and the cloud API key through the secret portal, which libsecret uses by itself in a sandbox.

Inside the sandbox the application is installed into `/app`, which is also `STAPIK_APP_SYSTEM_PREFIX`; the launcher
is then not installed, because it would take the place of the executable. Flatpak exports only files that start with
the application id, so the icon is installed as `<APPLICATION_ID>.svg` (or `.png`) and the desktop entry refers to it
by that name.
