# Money entry

`stapik/ui/widget/MoneyEntry.hpp` is a `Gtk::Entry` for amounts. The text is checked with `domain::parseAmount`, so
`12,50`, `12.50`, `1 234,56` and `1.234,56` are all read, and the result is an integer count of minor units.

```cpp
MoneyEntry entry(currencyCatalog.find("PLN"));
entry.signalAmountChanged().connect([&entry] { saveButton.set_sensitive(entry.money().has_value()); });

entry.setAmount(1250);          // shows 12,50 in Polish and 12.50 in English
const auto money = entry.money(); // std::optional<domain::Money>
```

## Behaviour

- Empty input is valid and gives no amount. Text that `parseAmount` rejects gets the `stapik-amount-invalid` CSS
  class and a tooltip with an example in the current language (`money.entry.invalid`).
- The number of fraction digits comes from `Currency::decimalPlaces`; more digits than that are rejected.
- A valid amount is rewritten with the separators of the current language when the field loses focus or Enter is
  pressed, without thousands grouping so it stays easy to edit. The same happens when the language changes.
- The placeholder shows zero in the current language (`0,00`, `0.00`).

`hasValidInput()` tells an empty field from a mistyped one, which `amount()` and `money()` both report as no value.
