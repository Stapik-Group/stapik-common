# CSV export

`stapik/csv/CsvWriter.hpp` writes tables as CSV following RFC 4180: records end with CRLF, a field is quoted when it
contains the separator, a quote or a line break, and quotes inside a quoted field are doubled. A row made of one empty
field is written as `""`, so it is not mistaken for a blank line.

```cpp
using namespace stapik::csv;

CsvWriter writer(CsvOptions::forExcel(LocaleManager::instance().languageCode()));
writer.addRow({ "Name", "Amount" });
writer.addRow({ "Milk, 2%", domain::formatAmount(350, 2, "pl", false) });

if (const auto saved = writer.saveTo(path); !saved)
    log::warning("Export failed: {}", saved.error().message());
```

`saveTo` writes atomically (see `writeFileAtomically`) and returns a `FileResult`.

## Options

| Option          | Default | Meaning                                                        |
|-----------------|---------|----------------------------------------------------------------|
| `separator`     | `Comma` | `Comma` or `Semicolon`                                         |
| `byteOrderMark` | `false` | UTF-8 byte order mark at the start, so Excel reads `ąęł` right |

`CsvOptions::forExcel(languageCode)` turns the byte order mark on and picks the separator Excel expects in that
language: `;` where the decimal separator is a comma (Polish, German), `,` otherwise. Format numbers for the same
language, for example with `domain::formatAmount(..., languageCode, false)`, so `12,50` and `;` go together.
