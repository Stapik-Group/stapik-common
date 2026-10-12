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
(`ł`, `đ`, `ø`), and spells out ligatures (`œ` = `oe`, `æ` = `ae`, `þ` = `th`, `ı` = `i`). Transliterations are not
matched: `Müller` is found by `muller`, not by `mueller`. Marks that belong to a letter in another script stay, so Cyrillic `й` is not `и`. Its result is
decomposed (NFKD), so use it for comparing only, never for display. Invalid UTF-8 is read as U+FFFD instead of failing.

## Sorting

`stapik/text/Collation.hpp` orders text by the alphabet of a language. `Glib::ustring::collate_key` is not used: it
follows the system locale (in the "C" locale `ł` sorts after `z`), while the order in a list should follow the language
chosen in the application.

```cpp
std::ranges::sort(names, TextLess(LocaleManager::instance().languageCode()));
// abc, ąbc, cukier, ćma, lato, Łódź, ogórek, olej, sad, śliwka, tort, zebra, zło, źle, żaba
```

`compareText(a, b, languageCode)` returns a negative number, zero or a positive number. For long lists compute
`collationKey` once per item and sort the keys with plain `operator<`.

| Language | Diacritics                                                             |
|----------|------------------------------------------------------------------------|
| `pl`     | `ą ć ę ł ń ó ś ź ż` are separate letters: `a < ą < b`, `z < ź < ż`      |
| other    | tie-breakers only: `Lodz < Łódź < mama`; `ß` sorts as `ss`, `œ` as `oe` |

Only Polish has an alphabet of its own for now. English, German (`ä` as `a`, DIN 5007-1) and French (accents) come out
right with the default rules. Accents are compared from the start of the word, so `coté < côte`; traditional French
dictionaries compare them from the end of the word, which only matters for words that differ in nothing else. Languages in which a diacritic or a digraph is a letter of its own are not tailored yet:
Spanish `ñ`, Swedish, Danish and Norwegian `å ä ö æ ø`, Czech and Slovak (`č`, `ř`, `š`, `ch`), Turkish `ı ç ğ ş`.
They are sorted like English until their alphabet is added to `Collation.cpp`.

Letters decide first, then diacritics, then case with lower case first (`anna < Anna`). Symbols sort before digits,
digits before letters, and the letters of other alphabets (Cyrillic, Greek) after the Latin ones. Digits are compared
one by one, so `10` comes before `2`. Texts that differ only in the form of Unicode normalization compare equal.
