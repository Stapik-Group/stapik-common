#include "CurrencyCatalog.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AppPaths.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <utility>

namespace stapik::domain
{
    namespace
    {
        std::string uppercase(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });
            return text;
        }

        std::optional<Currency> parseCurrency(const nlohmann::json& entry)
        {
            if (!entry.is_object() || !entry.contains("code") || !entry.at("code").is_string())
                return std::nullopt;

            Currency currency;
            currency.code = uppercase(entry.at("code").get<std::string>());
            if (currency.code.empty())
                return std::nullopt;

            currency.symbol = currency.code;
            if (entry.contains("symbol") && entry.at("symbol").is_string())
                currency.symbol = entry.at("symbol").get<std::string>();

            if (entry.contains("symbolBeforeAmount"))
                currency.symbolBeforeAmount = entry.at("symbolBeforeAmount").get<bool>();

            if (entry.contains("decimalPlaces"))
                currency.decimalPlaces = entry.at("decimalPlaces").get<int>();

            if (currency.decimalPlaces < 0 || currency.decimalPlaces > MAX_DECIMAL_PLACES)
                return std::nullopt;

            return currency;
        }
    }

    CurrencyCatalog& CurrencyCatalog::instance()
    {
        static CurrencyCatalog catalog = []
        {
            CurrencyCatalog result;
            result.addFromFile(AppPaths::commonResourcesDir() / "currencies.json");
            result.addFromFile(AppPaths::resourcesDir() / "currencies.json");
            return result;
        }();

        return catalog;
    }

    bool CurrencyCatalog::addFromFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        if (!stream.is_open())
        {
            log::debug("Currency file not found: {}", file.string());
            return false;
        }

        nlohmann::json json;
        try
        {
            json = nlohmann::json::parse(stream);
        }
        catch (const nlohmann::json::exception& exception)
        {
            log::warning("Cannot parse currencies from {}: {}", file.string(), exception.what());
            return false;
        }

        if (!json.is_array())
        {
            log::warning("Currency file {} must contain a JSON array", file.string());
            return false;
        }

        for (std::size_t index = 0; index < json.size(); ++index)
        {
            std::optional<Currency> currency;
            try
            {
                currency = parseCurrency(json.at(index));
            }
            catch (const nlohmann::json::exception&)
            {
                currency.reset();
            }

            if (currency)
                add(std::move(*currency));
            else
                log::warning("Ignoring invalid currency entry {} in {}", index, file.string());
        }

        return true;
    }

    void CurrencyCatalog::add(Currency currency)
    {
        if (const auto existing = std::ranges::find(m_currencies, currency.code, &Currency::code); existing != m_currencies.end())
            *existing = std::move(currency);
        else
            m_currencies.push_back(std::move(currency));
    }

    const std::vector<Currency>& CurrencyCatalog::currencies() const
    {
        return m_currencies;
    }

    const Currency* CurrencyCatalog::find(const std::string_view code) const
    {
        const auto currency = std::ranges::find_if(m_currencies, [code](const Currency& candidate)
        {
            return candidate.code == code;
        });

        return currency == m_currencies.end() ? nullptr : std::to_address(currency);
    }
}
