#pragma once

#include "stapik/domain/Currency.hpp"
#include "stapik/domain/Money.hpp"

#include <gtkmm/entry.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include <optional>
#include <string>

class MoneyEntry : public Gtk::Entry
{
public:
    explicit MoneyEntry(stapik::domain::Currency currency = {});
    ~MoneyEntry() override;

    MoneyEntry(const MoneyEntry&) = delete;
    MoneyEntry& operator=(const MoneyEntry&) = delete;

    void setCurrency(const stapik::domain::Currency& currency);
    [[nodiscard]] const stapik::domain::Currency& currency() const;

    void setAmount(std::optional<std::int64_t> minorUnits);
    [[nodiscard]] std::optional<std::int64_t> amount() const;
    [[nodiscard]] std::optional<stapik::domain::Money> money() const;
    [[nodiscard]] bool hasValidInput() const;

    sigc::signal<void()>& signalAmountChanged();

private:
    [[nodiscard]] std::string trimmedText() const;
    [[nodiscard]] std::string exampleText() const;
    void normalizeText();
    void refreshLanguage();
    void refreshValidity();

    stapik::domain::Currency m_currency;
    sigc::signal<void()> m_signalAmountChanged;
    sigc::connection m_localeConnection;
};
