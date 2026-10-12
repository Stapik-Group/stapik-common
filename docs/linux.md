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
