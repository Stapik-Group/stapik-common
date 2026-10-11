#include "Collation.hpp"

#include "TextFolding.hpp"
#include "stapik/locale/LanguageCode.hpp"

#include <glib.h>

#include <array>
#include <optional>
#include <vector>

namespace stapik::text
{
    namespace
    {
        // Every weight in a level is above the separator, so a shorter text sorts before its continuations.
        constexpr char LEVEL_SEPARATOR = '\x01';
        constexpr char NO_MARK = '\x02';
        constexpr char LOWER_CASE = '\x02';
        constexpr char UPPER_CASE = '\x03';

        // Primary weights, in the order: punctuation, digits, letters, everything else (as UTF-8, from 0xC2).
        constexpr unsigned DIGIT_WEIGHT = 0x50;
        constexpr unsigned LETTER_WEIGHT = 0x60;
        constexpr unsigned FIRST_MARK_WEIGHT = 0x03;

        constexpr char32_t OGONEK = 0x0328;
        constexpr char32_t ACUTE = 0x0301;
        constexpr char32_t DOT_ABOVE = 0x0307;
        constexpr char32_t STROKE = 0x0335;

        struct Letter
        {
            char32_t base;
            char32_t mark;
        };

        constexpr std::array POLISH_ALPHABET = {
            Letter{.base = U'a', .mark = 0},
            Letter{.base = U'a', .mark = OGONEK},
            Letter{.base = U'b', .mark = 0},
            Letter{.base = U'c', .mark = 0},
            Letter{.base = U'c', .mark = ACUTE},
            Letter{.base = U'd', .mark = 0},
            Letter{.base = U'e', .mark = 0},
            Letter{.base = U'e', .mark = OGONEK},
            Letter{.base = U'f', .mark = 0},
            Letter{.base = U'g', .mark = 0},
            Letter{.base = U'h', .mark = 0},
            Letter{.base = U'i', .mark = 0},
            Letter{.base = U'j', .mark = 0},
            Letter{.base = U'k', .mark = 0},
            Letter{.base = U'l', .mark = 0},
            Letter{.base = U'l', .mark = STROKE},
            Letter{.base = U'm', .mark = 0},
            Letter{.base = U'n', .mark = 0},
            Letter{.base = U'n', .mark = ACUTE},
            Letter{.base = U'o', .mark = 0},
            Letter{.base = U'o', .mark = ACUTE},
            Letter{.base = U'p', .mark = 0},
            Letter{.base = U'q', .mark = 0},
            Letter{.base = U'r', .mark = 0},
            Letter{.base = U's', .mark = 0},
            Letter{.base = U's', .mark = ACUTE},
            Letter{.base = U't', .mark = 0},
            Letter{.base = U'u', .mark = 0},
            Letter{.base = U'v', .mark = 0},
            Letter{.base = U'w', .mark = 0},
            Letter{.base = U'x', .mark = 0},
            Letter{.base = U'y', .mark = 0},
            Letter{.base = U'z', .mark = 0},
            Letter{.base = U'z', .mark = ACUTE},
            Letter{.base = U'z', .mark = DOT_ABOVE},
        };

        struct Cluster
        {
            char32_t base;
            std::u32string marks;
            bool upperCase;
        };

        struct PrimaryLetter
        {
            unsigned rank;
            std::size_t usedMarks;
        };

        std::u32string foldedCodePoints(const char32_t codePoint)
        {
            if (codePoint < 0x80)
                return std::u32string(1, static_cast<char32_t>(g_ascii_tolower(static_cast<gchar>(codePoint))));

            return decompose(foldCase(toUtf8(std::u32string(1, codePoint))));
        }

        std::vector<Cluster> clustersOf(const std::string_view text)
        {
            std::vector<Cluster> clusters;

            for (const auto codePoint : decompose(text))
            {
                const auto unicode = static_cast<gunichar>(codePoint);
                const bool upperCase = g_unichar_isupper(unicode) || g_unichar_istitle(unicode);

                for (const auto folded : foldedCodePoints(codePoint))
                {
                    if (isCombiningMark(folded) && !clusters.empty())
                        clusters.back().marks.push_back(folded);
                    else
                        clusters.push_back({.base = folded, .marks = {}, .upperCase = upperCase});
                }
            }
            return clusters;
        }

        std::optional<PrimaryLetter> primaryLetter(const bool polish, const Cluster& cluster)
        {
            if (!polish)
            {
                if (cluster.base >= U'a' && cluster.base <= U'z')
                    return PrimaryLetter{.rank = cluster.base - U'a', .usedMarks = 0};
                return std::nullopt;
            }

            const auto rankOf = [](const char32_t base, const char32_t mark) -> std::optional<unsigned>
            {
                for (unsigned rank = 0; rank < POLISH_ALPHABET.size(); ++rank)
                {
                    if (POLISH_ALPHABET[rank].base == base && POLISH_ALPHABET[rank].mark == mark)
                        return rank;
                }
                return std::nullopt;
            };

            if (!cluster.marks.empty())
            {
                if (const auto rank = rankOf(cluster.base, cluster.marks.front()))
                    return PrimaryLetter{.rank = *rank, .usedMarks = 1};
            }

            if (const auto rank = rankOf(cluster.base, 0))
                return PrimaryLetter{.rank = *rank, .usedMarks = 0};
            return std::nullopt;
        }

        unsigned asciiSymbolWeight(const char32_t codePoint)
        {
            if (codePoint <= 0x2F)
                return codePoint < 0x02 ? 0x02 : static_cast<unsigned>(codePoint);
            if (codePoint <= 0x40)
                return static_cast<unsigned>(codePoint) - 0x0A;
            if (codePoint <= 0x60)
                return static_cast<unsigned>(codePoint) - 0x24;
            return static_cast<unsigned>(codePoint) - 0x3E;
        }

        void appendOtherPrimary(std::string& primary, const char32_t codePoint)
        {
            if (codePoint >= U'0' && codePoint <= U'9')
                primary.push_back(static_cast<char>(DIGIT_WEIGHT + (codePoint - U'0')));
            else if (codePoint < 0x80)
                primary.push_back(static_cast<char>(asciiSymbolWeight(codePoint)));
            else
                primary += toUtf8(std::u32string(1, codePoint));
        }
    }

    std::string collationKey(const std::string_view text, const std::string_view languageCode)
    {
        const bool polish = locale::primaryLanguage(languageCode) == "pl";

        std::string primary;
        std::string secondary;
        std::string tertiary;

        for (const auto& cluster : clustersOf(text))
        {
            std::size_t usedMarks = 0;
            if (const auto letter = primaryLetter(polish, cluster))
            {
                primary.push_back(static_cast<char>(LETTER_WEIGHT + letter->rank));
                usedMarks = letter->usedMarks;
            } else
            {
                appendOtherPrimary(primary, cluster.base);
            }

            for (std::size_t index = usedMarks; index < cluster.marks.size(); ++index)
                secondary.push_back(static_cast<char>(FIRST_MARK_WEIGHT + (cluster.marks[index] - 0x0300)));
            secondary.push_back(NO_MARK);

            tertiary.push_back(cluster.upperCase ? UPPER_CASE : LOWER_CASE);
        }

        return primary + LEVEL_SEPARATOR + secondary + LEVEL_SEPARATOR + tertiary;
    }

    int compareText(const std::string_view first, const std::string_view second, const std::string_view languageCode)
    {
        const auto result = collationKey(first, languageCode).compare(collationKey(second, languageCode));
        return result < 0 ? -1 : (result > 0 ? 1 : 0);
    }

    TextLess::TextLess(std::string languageCode)
        : m_languageCode(std::move(languageCode))
    {
    }

    bool TextLess::operator()(const std::string_view first, const std::string_view second) const
    {
        return compareText(first, second, m_languageCode) < 0;
    }
}
