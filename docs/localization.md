# Localization

Translations are JSON files named after the language code (`en.json`, `pl.json`). Applications add their own files
next to the common ones; an application key overrides a common key with the same name, and a key missing in the
selected language falls back to English.

## Languages

A language is identified by its code, which is the name of its JSON file without the extension. Whatever files are
found in the locales directories are the available languages, so adding `fr.json` is enough to offer French.
`language.name` in the file is the name shown in the language menu.

```cpp
auto& loc = LocaleManager::instance();
loc.languages();         // codes and display names of all found languages
loc.setLanguage("fr");
loc.languageCode();      // "fr"
```

The selected code is stored in the `locale` setting. A legacy `locale.txt` is imported in lower case.

## Placeholders

`{name}` in a translation is replaced with the argument of the same name. Unknown placeholders are left as they are.

```cpp
LocaleManager::instance().translate("greeting", { { "name", "Ola" } });
```

## Plural forms

A plural message has one entry per CLDR category: `<key>.one`, `<key>.few`, `<key>.many` and `<key>.other`
(`zero` and `two` are available for languages that need them). `translatePlural` picks the entry that the rules of the
selected language assign to the number.

```json
{
  "transactions.one": "{count} transakcja",
  "transactions.few": "{count} transakcje",
  "transactions.many": "{count} transakcji"
}
```

```cpp
loc.translatePlural("transactions", 1);   // 1 transakcja
loc.translatePlural("transactions", 3);   // 3 transakcje
loc.translatePlural("transactions", 12);  // 12 transakcji
```

- `{count}` is filled in automatically. Passing a `count` argument explicitly replaces it, e.g. with a formatted
  number: `translatePlural("transactions", n, { { "count", formatted } })`.
- A missing category falls back to `<key>.other`. If the selected language has neither, English is tried, using the
  English rules for the number.
- Negative numbers are treated by their absolute value.

| Languages                                | Categories used for whole numbers                 |
|------------------------------------------|---------------------------------------------------|
| `pl`                                     | `one` (1), `few` (2–4, 22–24…), `many`            |
| `ru`, `uk`                               | `one` (1, 21…), `few`, `many`                     |
| `hr`, `sr`, `bs`                         | `one` (1, 21…), `few`, `other`                    |
| `cs`, `sk`                               | `one` (1), `few` (2–4), `other`                   |
| `fr`, `pt`                               | `one` (0 and 1), `other`, `many` (whole millions) |
| `es`, `it`, `ca`                         | `one` (1), `other`, `many` (whole millions)       |
| `ja`, `zh`, `ko`, `vi`, `th`, `id`, `ms` | `other`                                           |
| everything else, including `en`, `de`    | `one` (1), `other`                                |

Rules are keyed by the language part of the code, so `pt-BR` follows `pt`. Fractional numbers are not supported.

## Dates and times

`stapik/locale/DateFormatter.hpp` formats dates and times by the language code, like `formatMoney` does for amounts.
The functions take a wall-clock time (`LocalSeconds`, a `std::chrono::local_seconds`), so they do not depend on a
time zone database. `toLocalTime` converts a `system_clock::time_point` to the local time of the user.

```cpp
using namespace stapik::locale;

const auto when = toLocalTime(std::chrono::system_clock::now());
formatDate(when, loc.languageCode());       // 30.09.2026
formatTime(when, loc.languageCode());       // 14:05
formatDateTime(when, loc.languageCode());   // 30.09.2026 14:05
```

| Languages  | Date         | Time      | Date and time         |
|------------|--------------|-----------|-----------------------|
| `pl`, `de` | `30.09.2026` | `14:05`   | `30.09.2026 14:05`    |
| other      | `09/30/2026` | `2:05 PM` | `09/30/2026, 2:05 PM` |

`formatTime` and `formatDateTime` take `withSeconds` as the last argument. The space before `AM`/`PM` is a no-break
space. A `std::chrono::year_month_day` can be passed to `formatDate` directly, e.g. `YearMonth::firstDay()`.

### Relative time

`stapik/locale/RelativeTime.hpp` turns a time into "5 minutes ago", "yesterday", "3 days ago". It needs the translations,
so it takes a `LocaleManager` (or a `LocalizationEngine`). Like the functions above it works on local wall-clock time;
`now` is passed in, which keeps the result testable.

```cpp
formatRelativeTime(std::chrono::system_clock::now() - std::chrono::hours{3}, LocaleManager::instance());  // 3 godziny temu
formatRelativeTime(when, now, LocaleManager::instance());                                              // LocalSeconds
```

| Distance                                  | pl                            | en                     |
|-------------------------------------------|-------------------------------|------------------------|
| under a minute                            | przed chwilą / za chwilę      | just now / in a moment |
| under an hour                             | 5 minut temu / za 5 minut     | 5 minutes ago          |
| under a day (elapsed time, not the date)  | 2 godziny temu / za 2 godziny | 2 hours ago            |
| one calendar day                          | wczoraj / jutro               | yesterday / tomorrow   |
| up to `maxRelativeDays` (6) calendar days | 3 dni temu / za 3 dni         | 3 days ago             |
| further                                   | `30.09.2026` (`formatDate`)   | `09/30/2026`           |

`RelativeTimeOptions::maxRelativeDays` moves the last limit. `describeRelativeTime` returns the same decision as
data (`RelativeTimeKind` and a count) for callers that render it themselves.

The messages are the plural keys `time.relative.<minutes|hours|days><Ago|Ahead>.<category>` plus `justNow`, `soon`,
`yesterday` and `tomorrow`. A new language has to provide all of them; `CommonResourcesTest` checks that every plural
message has a form for each number its language can produce.
