# Update checker

Applications built on `StapikApplication` can tell the user that a newer version is available. Nothing is downloaded
or installed: the dialog only offers to open the release page in the browser.

Enable it in the application options:

```cpp
stapik::app::StapikApplicationOptions options;
options.checkForUpdates = true;
```

## How it works

- The repository is taken from `AppInfo::repositoryUrl`, which has to be a `https://github.com/<owner>/<repo>` URL.
- `AppInfo::version` and the tag of the latest GitHub release (`v1.2.3` or `1.2.3`) are compared as
  [Semantic Versions](https://semver.org). Prereleases and drafts are not offered, because GitHub does not return
  them as the latest release. If either side is not a semantic version, the check is skipped and a warning is logged.
- The request goes to `api.github.com` in the background, through the same TLS restrictions as the cloud client.
- A check is made at most once every 24 hours, and a given version is announced only once. A failed lookup is not
  counted, so it is retried on the next start.

## Settings

| Key | Meaning |
| --- | --- |
| `update.checkEnabled` | `false` turns the check off. Defaults to `true`; applications should expose it in their settings. |
| `update.lastCheck` | Time of the last successful lookup. |
| `update.announcedVersion` | The version the user was already told about. |

## Without `StapikApplication`

`stapik::update::UpdateChecker` can be used on its own: connect to `signalUpdateAvailable()` and call `check()`.
`showUpdateAvailableDialog()` is the dialog used by `StapikApplication`.
