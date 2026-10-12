# Tutorial

`showTutorialWindow()` opens a help window: a contents tree on the left, the chapter
on the right, *Previous* / *Next* buttons and a button that hides the contents. stapik-common provides the window and
the loader; the text of the tutorial lives in the application, as Markdown files.

## Hooking it up

Give `StandardMenu` the directory with the tutorial. *Help → Tutorial* (F1) appears and opens the window:

```cpp
StandardMenuOptions options;
options.tutorialDirectory = AppPaths::resourcesDir() / "tutorial";
StandardMenu menu(window, options);
```

An empty `tutorialDirectory` (the default) leaves the item out. `StandardMenu::installShortcuts()` binds F1.

The window can also be opened from the code, optionally on a chapter, for example to show the tutorial on the first run:

```cpp
showTutorialWindow(window, AppPaths::resourcesDir() / "tutorial");
showTutorialWindow(window, AppPaths::resourcesDir() / "tutorial", "getting-started/first-steps");
```

There is one tutorial window: calling it again brings the open one to the front. The window follows the language
(it reloads the text when the language changes) and remembers the width of the contents pane under
`window.panes.tutorial`.

## Layout of the files

One folder per language, named like the language code (`pl`, `en`, `de`). A language without a folder falls back to
`en`.

```
resources/tutorial/
├── en/
│   ├── 01-getting-started/
│   │   ├── index.md
│   │   ├── 01-first-steps.md
│   │   └── 02-saving.md
│   ├── 02-tips.md
│   └── images/
│       └── main-window.png
└── pl/
    └── ...
```

| Item                            | Becomes                                                                                          |
|---------------------------------|--------------------------------------------------------------------------------------------------|
| `*.md` file                     | a page                                                                                           |
| folder                          | a book: it has pages inside and can be expanded                                                  |
| `index.md` of a folder          | the page of the book. Without it the book shows a generated list of its pages.                   |
| anything else, `.hidden` names  | ignored. A folder without pages (`images/`) does not show up.                                    |

* Entries are sorted by file name. A prefix like `01-` only sets the order and is not part of the id; use two digits.
* The id of a chapter is its path without prefixes and extension: `01-getting-started/02-saving.md` is
  `getting-started/saving`. It is what `showTutorialWindow()` and links refer to.
* The title is the first heading of the file. Without a heading it is made from the file name.
* Files are UTF-8.

`stapik::tutorial::loadTutorial(directory, languageCode)` returns the `Tutorial` tree (chapters, `find`, `next`,
`previous`) without any GTK, which is also what the tests use.

## Markdown

A small subset is supported:

| Syntax                                   | Result                                                                    |
|------------------------------------------|---------------------------------------------------------------------------|
| `#` to `######`                          | headings (the first three levels differ in size)                          |
| blank line                               | ends a paragraph; lines of one paragraph are joined                       |
| `-`, `*`, `+` or `1.`                    | list items; two spaces of indent per nesting level                        |
| `> text`                                 | quote, good for tips                                                      |
| ` ``` ` fence                            | code block, shown as is                                                   |
| `![description](images/main-window.png)` | image, alone on a line; the path is relative to the file                  |
| `---`                                    | separator                                                                 |
| `**bold**`, `*italic*`, `` `code` ``     | inline styles; `\*` shows a literal marker                                |
| `[text](https://...)`                    | link to the web, opened in the browser (`http`, `https` and `mailto`)     |
| `[text](#getting-started/saving)`        | link to another chapter by id                                             |

Tables, HTML, `_italic_` and reference links are not supported. If an image cannot be loaded, its description is
shown instead and a warning is logged.

## Pieces

| Header                                | What it does                                                         |
|---------------------------------------|----------------------------------------------------------------------|
| `stapik/markdown/MarkdownParser.hpp`  | `parseMarkdown()` and `parseInline()`, no dependency on GTK          |
| `stapik/tutorial/TutorialLoader.hpp`  | `loadTutorial()`, `findLanguageDirectory()`                          |
| `stapik/ui/widget/MarkdownView.hpp`   | a read-only view of a Markdown document, emits `signalLinkActivated` |
| `stapik/ui/dialog/TutorialWindow.hpp` | `showTutorialWindow()`                                               |

Translation keys of the window: `menu.help.tutorial`, `dialog.tutorial.title` (`{name}` is the application name),
`dialog.tutorial.contents`, `dialog.tutorial.previous`, `dialog.tutorial.next`, `dialog.tutorial.empty`.
