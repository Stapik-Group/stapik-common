# Text search and sorting

`stapik/text/TextFolding.hpp` prepares text for comparing. It uses GLib for Unicode data, which does not depend on
the system locale.

```cpp
using namespace stapik::text;

foldCase("ŁÓDŹ");                        // łódź
foldForSearch("Zażółć gęślą jaźń");      // zazolc gesla jazn
containsFolded("Kraków", "KRAKOW");      // true
```

`containsFolded` is enough for a search box over a list. When the same query is checked against many items, fold
it once with `foldForSearch` and compare with `std::string::find`.

`foldForSearch` removes the diacritics of Latin and Greek letters, including the letters Unicode does not decompose
(`ł`, `đ`, `ø`). Marks that belong to a letter in another script stay, so Cyrillic `й` is not `и`. Its result is
decomposed (NFKD), so use it for comparing only, never for display. Invalid UTF-8 is read as U+FFFD instead of failing.
