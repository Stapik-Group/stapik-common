#include "PluralRules.hpp"

#include "LanguageCode.hpp"

namespace stapik::locale
{
    namespace
    {
        using Count = unsigned long long;
        using Rule = PluralCategory (*)(Count);

        bool inRange(const Count value, const Count first, const Count last)
        {
            return value >= first && value <= last;
        }

        PluralCategory noPlurals(Count)
        {
            return PluralCategory::Other;
        }

        PluralCategory oneOther(const Count count)
        {
            return count == 1 ? PluralCategory::One : PluralCategory::Other;
        }

        PluralCategory oneForZeroAndOne(const Count count)
        {
            using enum PluralCategory;
            if (count <= 1)
                return One;

            return count % 1000000 == 0 ? Many : Other;
        }

        PluralCategory oneWithMillions(const Count count)
        {
            using enum PluralCategory;
            if (count == 1)
                return One;

            return count != 0 && count % 1000000 == 0 ? Many : Other;
        }

        PluralCategory polish(const Count count)
        {
            using enum PluralCategory;
            if (count == 1)
                return One;

            if (inRange(count % 10, 2, 4) && !inRange(count % 100, 12, 14))
                return Few;

            return Many;
        }

        PluralCategory eastSlavic(const Count count)
        {
            using enum PluralCategory;
            if (count % 10 == 1 && count % 100 != 11)
                return One;

            if (inRange(count % 10, 2, 4) && !inRange(count % 100, 12, 14))
                return Few;

            return Many;
        }

        PluralCategory southSlavic(const Count count)
        {
            using enum PluralCategory;
            if (count % 10 == 1 && count % 100 != 11)
                return One;

            if (inRange(count % 10, 2, 4) && !inRange(count % 100, 12, 14))
                return Few;

            return Other;
        }

        PluralCategory czechSlovak(const Count count)
        {
            using enum PluralCategory;
            if (count == 1)
                return One;

            return inRange(count, 2, 4) ? Few : Other;
        }

        struct LanguageRule
        {
            std::string_view language;
            Rule rule;
        };

        // Languages missing here follow the English rule.
        constexpr LanguageRule RULES[] = {
            { .language = "bs", .rule = southSlavic },
            { .language = "ca", .rule = oneWithMillions },
            { .language = "cs", .rule = czechSlovak },
            { .language = "es", .rule = oneWithMillions },
            { .language = "fr", .rule = oneForZeroAndOne },
            { .language = "hr", .rule = southSlavic },
            { .language = "id", .rule = noPlurals },
            { .language = "it", .rule = oneWithMillions },
            { .language = "ja", .rule = noPlurals },
            { .language = "ko", .rule = noPlurals },
            { .language = "ms", .rule = noPlurals },
            { .language = "pl", .rule = polish },
            { .language = "pt", .rule = oneForZeroAndOne },
            { .language = "ru", .rule = eastSlavic },
            { .language = "sk", .rule = czechSlovak },
            { .language = "sr", .rule = southSlavic },
            { .language = "th", .rule = noPlurals },
            { .language = "uk", .rule = eastSlavic },
            { .language = "vi", .rule = noPlurals },
            { .language = "zh", .rule = noPlurals },
        };

        Rule ruleFor(const std::string_view languageCode)
        {
            const auto language = primaryLanguage(languageCode);
            for (const auto& [name, rule] : RULES)
            {
                if (name == language)
                    return rule;
            }
            return oneOther;
        }
    }

    PluralCategory pluralCategory(const std::string_view languageCode, const long long count)
    {
        const auto magnitude = count < 0 ? 0ULL - static_cast<Count>(count) : static_cast<Count>(count);
        return ruleFor(languageCode)(magnitude);
    }

    std::string_view pluralCategoryName(const PluralCategory category)
    {
        switch (category)
        {
            using enum PluralCategory;
            case Zero: return "zero";
            case One: return "one";
            case Two: return "two";
            case Few: return "few";
            case Many: return "many";
            case Other: return "other";
        }
        return "other";
    }
}
