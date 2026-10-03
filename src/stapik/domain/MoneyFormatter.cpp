#include "MoneyFormatter.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <limits>
#include <vector>

namespace stapik::domain
{
    namespace
    {
        constexpr std::string_view NO_BREAK_SPACE = "\xC2\xA0";
        constexpr std::string_view NARROW_NO_BREAK_SPACE = "\xE2\x80\xAF";

        std::string languageOf(const std::string_view languageCode)
        {
            std::string language(languageCode.substr(0, languageCode.find_first_of("_-.@")));
            std::ranges::transform(language, language.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return language;
        }

        std::uint64_t magnitudeOf(const std::int64_t value)
        {
            return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
        }

        std::uint64_t powerOfTen(const int exponent)
        {
            std::uint64_t result = 1;
            for (int step = 0; step < exponent; ++step)
                result *= 10;
            return result;
        }

        std::string formatMagnitude(
            const std::uint64_t magnitude,
            const int decimalPlaces,
            const NumberFormat &format,
            const bool groupThousands)
        {
            const auto factor = powerOfTen(decimalPlaces);
            const auto digits = std::format("{}", magnitude / factor);

            std::string integerPart;
            integerPart.reserve(digits.size() + digits.size() / 3);

            for (std::size_t index = 0; index < digits.size(); ++index)
            {
                if (groupThousands && index > 0 && (digits.size() - index) % 3 == 0)
                    integerPart += format.groupSeparator;

                integerPart += digits[index];
            }

            if (decimalPlaces == 0)
                return integerPart;

            return std::format(
                "{}{}{:0{}}",
                integerPart,
                format.decimalSeparator,
                magnitude % factor,
                decimalPlaces);
        }

        bool consumeSpace(const std::string_view text, std::size_t &position)
        {
            if (text[position] == ' ' || text[position] == '\t')
            {
                ++position;
                return true;
            }

            for (const auto space: {NO_BREAK_SPACE, NARROW_NO_BREAK_SPACE})
            {
                if (text.substr(position, space.size()) == space)
                {
                    position += space.size();
                    return true;
                }
            }

            return false;
        }

        std::vector<std::string_view> split(const std::string_view text, const char separator)
        {
            std::vector<std::string_view> parts;

            std::size_t start = 0;
            while (true)
            {
                const auto end = text.find(separator, start);
                if (end == std::string_view::npos)
                {
                    parts.push_back(text.substr(start));
                    return parts;
                }

                parts.push_back(text.substr(start, end - start));
                start = end + 1;
            }
        }

        std::optional<std::string> integerDigits(const std::string_view raw, const char groupSeparator)
        {
            if (groupSeparator == '\0')
                return std::string(raw);

            const auto parts = split(raw, groupSeparator);

            std::string digits;
            for (std::size_t index = 0; index < parts.size(); ++index)
            {
                if (const bool validSize = index == 0
                                               ? !parts[index].empty() && parts[index].size() <= 3
                                               : parts[index].size() == 3;
                    !validSize)
                {
                    return std::nullopt;
                }

                digits += parts[index];
            }

            return digits;
        }

        std::optional<std::uint64_t> parseDigits(const std::string_view digits)
        {
            std::uint64_t value = 0;

            for (const char character: digits)
            {
                const auto digit = static_cast<std::uint64_t>(character - '0');
                if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                    return std::nullopt;

                value = value * 10 + digit;
            }

            return value;
        }

        std::optional<std::pair<char, char> > detectSeparators(
            const std::string_view body,
            const int decimalPlaces)
        {
            const auto dots = static_cast<std::size_t>(std::ranges::count(body, '.'));
            const auto commas = static_cast<std::size_t>(std::ranges::count(body, ','));

            if (dots > 0 && commas > 0)
            {
                const char decimalSeparator =
                        body.rfind('.') > body.rfind(',') ? '.' : ',';
                const char groupSeparator =
                        decimalSeparator == '.' ? ',' : '.';

                if (const auto decimalPosition = body.rfind(decimalSeparator);
                    std::ranges::count(body, decimalSeparator) != 1 || body.rfind(groupSeparator) > decimalPosition)
                    return std::nullopt;

                return std::pair{decimalSeparator, groupSeparator};
            }

            if (dots + commas == 1 && decimalPlaces > 0)
                return std::pair{dots == 1 ? '.' : ',', '\0'};

            if (dots + commas >= 1)
                return std::pair{'\0', dots > 0 ? '.' : ','};

            return std::pair{'\0', '\0'};
        }

        bool validAmountCharacters(const std::string_view text)
        {
            return std::ranges::all_of(text, [](const char character)
            {
                return (character >= '0' && character <= '9') ||
                       character == '.' || character == ',';
            });
        }

        std::string removeSpaces(const std::string_view text)
        {
            std::string result;
            for (std::size_t position = 0; position < text.size();)
            {
                if (!consumeSpace(text, position))
                    result += text[position++];
            }

            return result;
        }
    }

    NumberFormat numberFormatFor(const std::string_view languageCode)
    {
        const auto language = languageOf(languageCode);

        if (language == "pl")
            return {.decimalSeparator = ",", .groupSeparator = std::string(NO_BREAK_SPACE)};

        if (language == "de")
            return {.decimalSeparator = ",", .groupSeparator = "."};

        return {.decimalSeparator = ".", .groupSeparator = ","};
    }

    std::string formatAmount(
        const std::int64_t minorUnits,
        const int decimalPlaces,
        const std::string_view languageCode,
        const bool groupThousands)
    {
        const auto sign = minorUnits < 0 ? std::string("-") : std::string();
        return sign + formatMagnitude(magnitudeOf(minorUnits), decimalPlaces, numberFormatFor(languageCode),
                                      groupThousands);
    }

    std::string formatMoney(const Money &money, const std::string_view languageCode, const MoneyFormatOptions options)
    {
        const auto &currency = money.currency();
        const auto sign = money.isNegative() ? std::string("-") : std::string();
        const auto number = formatMagnitude(
            magnitudeOf(money.minorUnits()),
            currency.decimalPlaces,
            numberFormatFor(languageCode),
            options.groupThousands);

        if (!options.withSymbol || currency.symbol.empty())
            return sign + number;

        if (currency.symbolBeforeAmount)
            return sign + currency.symbol + number;

        return sign + number + std::string(NO_BREAK_SPACE) + currency.symbol;
    }

    std::optional<std::int64_t> parseAmount(const std::string_view text, const int decimalPlaces)
    {
        if (decimalPlaces < 0 || decimalPlaces > MAX_DECIMAL_PLACES)
            return std::nullopt;

        const auto cleaned = removeSpaces(text);

        bool negative = false;
        std::string_view body = cleaned;
        if (!body.empty() && (body.front() == '-' || body.front() == '+'))
        {
            negative = body.front() == '-';
            body.remove_prefix(1);
        }

        if (body.empty() || !validAmountCharacters(body))
            return std::nullopt;

        const auto separators = detectSeparators(body, decimalPlaces);
        if (!separators)
            return std::nullopt;

        const auto [decimalSeparator, groupSeparator] = *separators;

        std::string_view integerRaw = body;
        std::string_view fraction;
        if (decimalSeparator != '\0')
        {
            const auto position = body.rfind(decimalSeparator);
            integerRaw = body.substr(0, position);
            fraction = body.substr(position + 1);
        }

        const auto integer = integerDigits(integerRaw, groupSeparator);
        if (!integer || (integer->empty() && fraction.empty()) ||
            fraction.size() > static_cast<std::size_t>(decimalPlaces))
            return std::nullopt;

        std::string fractionDigits(fraction);
        fractionDigits.append(
            static_cast<std::size_t>(decimalPlaces) - fraction.size(), '0');

        const auto integerValue = parseDigits(*integer);
        const auto fractionValue = parseDigits(fractionDigits);
        if (!integerValue || !fractionValue)
            return std::nullopt;

        const auto factor = powerOfTen(decimalPlaces);
        if (*integerValue >
            (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - *fractionValue) / factor)
            return std::nullopt;

        const auto magnitude =
                static_cast<std::int64_t>(*integerValue * factor + *fractionValue);

        return negative ? -magnitude : magnitude;
    }
}
