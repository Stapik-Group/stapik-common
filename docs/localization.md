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
