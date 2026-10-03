#pragma once

#include "Currency.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace stapik::domain
{
    class CurrencyCatalog
    {
    public:
        CurrencyCatalog() = default;

        static CurrencyCatalog& instance();

        bool addFromFile(const std::filesystem::path& file);
        void add(Currency currency);

        [[nodiscard]] const std::vector<Currency>& currencies() const;
        [[nodiscard]] const Currency* find(std::string_view code) const;

    private:
        std::vector<Currency> m_currencies;
    };
}
