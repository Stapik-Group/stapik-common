# List view

`stapik/ui/list/ListColumnView.hpp` is the list every Stapik app ends up writing: a `Gio::ListStore` of items, a filter,
a sorter and a `Gtk::ColumnView` in a scrolled window. `ListColumnView<ItemType>` wires them together once.

`ItemType` is a `Glib::Object`. The columns read plain text from it.

```cpp
class ProductRow : public Glib::Object { /* name(), category(), ... */ };

stapik::ui::ListColumnView<ProductRow> list;
list.addColumn({ .titleKey = "products.name", .text = [](const ProductRow& row) { return row.name(); }, .expand = true });
list.addColumn({ .titleKey = "products.category", .text = [](const ProductRow& row) { return row.category(); } });
list.setSearchText([](const ProductRow& row) { return row.name(); });

list.setItems(rows);
list.signalActivated().connect([](const auto& row) { /* double click or Enter */ });
```

| Call                      | What it does                                                                                  |
|---------------------------|-----------------------------------------------------------------------------------------------|
| `addColumn`               | Adds a column. The title is a localization key and follows the language.                      |
| `setItems`, `append`      | Replaces or extends the content. `clear` empties it.                                          |
| `setQuery`                | Shows only items whose `setSearchText` text contains the query, ignoring case and diacritics. |
| `setPredicate`            | Shows only items the predicate accepts, together with the query.                              |
| `selectedItem`            | The selected item, or null.                                                                   |
| `visibleCount`            | Items left after filtering. `totalCount` counts all of them.                                  |
| `signalActivated`         | An item was activated.                                                                        |
| `signalSelectionChanged`  | The selection moved.                                                                          |

Clicking a column title sorts by that column's text. The comparison is the Polish-aware one of `stapik/text/Collation.hpp`
for the current language. `SearchEntry` fits `setQuery`: connect its `signalQueryChanged` to it.

Columns show text only. For anything else (a `CategoryChip`, a button) use `columnView()` and build the column by hand.
