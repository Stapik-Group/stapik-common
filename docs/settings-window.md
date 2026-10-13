# Settings window

`StandardMenu` offers *Settings → Settings…*, which opens a dialog with the language and the theme. The dialog is built
from a `SettingsLayout`: a list of rows, each bound to an `ObservableSetting`. A change is applied and saved at once,
and a change made elsewhere (for example a restored backup calling `reload()`) shows up in an open dialog.

```cpp
auto& settings = menu.settings();

settings.addBool("settings.autoSync", m_autoSync);
settings.addChoice("settings.dateFormat", m_dateFormat, {
    { DateFormat::Iso, "2026-10-12" },
    { DateFormat::Local, "12.10.2026" } });
settings.addNumber("settings.fontSize", m_fontSize, { .min = 8, .max = 32 });
settings.addPath("settings.backupFolder", m_backupFolder, stapik::ui::PathKind::Folder);
```

The first argument is the translation key of the row label.

| Row         | Setting                                                     | Control                                                           |
|-------------|-------------------------------------------------------------|-------------------------------------------------------------------|
| `addBool`   | `ObservableSetting<bool>`                                   | switch                                                            |
| `addChoice` | `ObservableSetting<T>` for any `T`                          | drop-down of `ChoiceOption<T>{ value, label }`                    |
| `addNumber` | `ObservableSetting<T>` for an integer or floating point `T` | spin button limited by `NumberRange<T>{ min, max, step, digits }` |
| `addFont`   | `ObservableSetting<std::string>`                            | button opening the font chooser of GTK, and a button for the font of the theme |
| `addPath`   | `ObservableSetting<std::filesystem::path>`                  | text field with a browse button (`PathKind::File` or `Folder`)    |

Labels of a choice are not translation keys. Pass a function instead of a list when they depend on the language; it is
called again when the language changes:

```cpp
settings.addChoice<Unit>("settings.unit", m_unit, [] {
    const auto& loc = LocaleManager::instance();
    return std::vector<ChoiceOption<Unit>>{
        { Unit::Metric, loc.translate("unit.metric") },
        { Unit::Imperial, loc.translate("unit.imperial") } };
});
```

`SettingsLayout` and `showSettingsDialog(parent, layout)` also work without `StandardMenu`. The menu item can be turned
off with `StandardMenuOptions::settingsItem`.

## Language and theme

The language and theme rows are added by `StandardMenu` itself (the theme row only when `StandardMenuOptions::themes`
is set). `LocaleManager::languageSetting()` and `ThemeManager::themeSetting()` give access to the underlying settings;
`signalLocaleChanged` and `signalThemeChanged` are emitted only when the value really changes.

## Font

`StandardMenu` adds a *Font* row: a button that opens the font chooser of GTK (a family and a size), and a *Default*
button, shown while a font is chosen, which goes back to the font and the sizes of the theme. The choice is kept as
text in the `font` setting, for example `Inter 11` (the family, then the size in points); an empty value means the
theme. `FontManager` holds it and emits `signalFontChanged`; `StapikApplication` re-applies the theme when it changes.
`stapik::theme::parseFontChoice` and `formatFontChoice` convert between the text and a `FontChoice`.

`AppStyleProvider::apply(themeId, font)` uses the chosen size as the base size of the theme: the first
`font-size: <n>px` of the theme file (the `*` rule) is the base, and every `font-size: <n>px` declaration is multiplied
by the chosen size (in CSS pixels, 96 per inch) divided by it, so the proportions between the sizes of a theme are
kept. Sizes in other units (`em`, `pt`, `%`) are not touched. The family is set by one more rule, `* { font-family }`,
loaded after the theme files. Sizes are limited to 6–40 pt.

The `fontScale` setting of earlier versions (a percentage) is no longer read.
