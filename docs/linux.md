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
