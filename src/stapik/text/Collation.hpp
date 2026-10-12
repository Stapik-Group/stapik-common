#pragma once

#include <string>
#include <string_view>

namespace stapik::text
{
    [[nodiscard]] std::string collationKey(std::string_view text, std::string_view languageCode);
    [[nodiscard]] int compareText(std::string_view first, std::string_view second, std::string_view languageCode);

    class TextLess
    {
    public:
        explicit TextLess(std::string languageCode);

        [[nodiscard]] bool operator()(std::string_view first, std::string_view second) const;

    private:
        std::string m_languageCode;
    };
}
