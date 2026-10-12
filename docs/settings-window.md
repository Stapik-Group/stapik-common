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

## Font size

The font size is a percentage scale of the theme's sizes (70–200 %, 100 % by default), stored in the `fontScale`
setting and edited in the window as a number row. `FontSizeManager` holds it and emits `signalFontSizeChanged`;
`StapikApplication` re-applies the theme when it changes.

`AppStyleProvider::apply(themeId, fontScalePercent)` scales every `font-size: <n>px` declaration of the theme files,
so the proportions between the sizes of a theme are kept. Sizes in other units (`em`, `pt`, `%`) are not touched.
