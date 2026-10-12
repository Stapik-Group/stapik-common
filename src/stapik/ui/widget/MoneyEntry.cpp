#include "MoneyEntry.hpp"

#include "stapik/domain/MoneyFormatter.hpp"
#include "stapik/locale/LocaleManager.hpp"
#include "stapik/text/Trim.hpp"

#include <gtkmm/eventcontrollerfocus.h>

#include <utility>

namespace
{
    constexpr auto INVALID_CSS_CLASS = "stapik-amount-invalid";
    constexpr std::int64_t EXAMPLE_MAJOR_UNITS = 12;

    std::string currentLanguage()
    {
        return LocaleManager::instance().languageCode();
    }
}

MoneyEntry::MoneyEntry(stapik::domain::Currency currency) :
    m_currency(std::move(currency))
{
    set_alignment(1.0F);
    set_input_purpose(Gtk::InputPurpose::NUMBER);

    signal_changed().connect([this]
    {
        refreshValidity();
        m_signalAmountChanged.emit();
    });
    signal_activate().connect([this] { normalizeText(); });

    const auto focusController = Gtk::EventControllerFocus::create();
    focusController->signal_leave().connect([this] { normalizeText(); });
    add_controller(focusController);

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { refreshLanguage(); });

    refreshLanguage();
}

MoneyEntry::~MoneyEntry()
{
    m_localeConnection.disconnect();
}

void MoneyEntry::setCurrency(const stapik::domain::Currency& currency)
{
    m_currency = currency;
    refreshLanguage();
    refreshValidity();
}

const stapik::domain::Currency& MoneyEntry::currency() const
{
    return m_currency;
}

void MoneyEntry::setAmount(const std::optional<std::int64_t> minorUnits)
{
    if (!minorUnits)
    {
        set_text("");
        return;
    }

    set_text(stapik::domain::formatAmount(*minorUnits, m_currency.decimalPlaces, currentLanguage(), false));
}

std::optional<std::int64_t> MoneyEntry::amount() const
{
    return stapik::domain::parseAmount(trimmedText(), m_currency.decimalPlaces);
}

std::optional<stapik::domain::Money> MoneyEntry::money() const
{
    const auto minorUnits = amount();
    if (!minorUnits)
        return std::nullopt;

    return stapik::domain::Money(*minorUnits, m_currency);
}

bool MoneyEntry::hasValidInput() const
{
    return trimmedText().empty() || amount().has_value();
}

sigc::signal<void()>& MoneyEntry::signalAmountChanged()
{
    return m_signalAmountChanged;
}

std::string MoneyEntry::trimmedText() const
{
    return stapik::text::trim(get_text().raw());
}

std::string MoneyEntry::exampleText() const
{
    const auto minorUnitsPerMajor = m_currency.minorUnitsPerMajor();
    const auto exampleMinorUnits = EXAMPLE_MAJOR_UNITS * minorUnitsPerMajor + minorUnitsPerMajor / 2;
    return stapik::domain::formatAmount(exampleMinorUnits, m_currency.decimalPlaces, currentLanguage(), false);
}

void MoneyEntry::normalizeText()
{
    if (const auto value = amount())
        setAmount(value);
}

void MoneyEntry::refreshLanguage()
{
    const auto current = amount();

    set_placeholder_text(stapik::domain::formatAmount(0, m_currency.decimalPlaces, currentLanguage(), false));

    if (current)
        setAmount(current);

    refreshValidity();
}

void MoneyEntry::refreshValidity()
{
    if (hasValidInput())
    {
        remove_css_class(INVALID_CSS_CLASS);
        set_tooltip_text({});
        return;
    }

    add_css_class(INVALID_CSS_CLASS);
    set_tooltip_text(LocaleManager::instance().translate("money.entry.invalid", {{"example", exampleText()}}));
}
