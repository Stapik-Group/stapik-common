#include "TextFolding.hpp"

#include <glib.h>

#include <array>
#include <memory>

namespace stapik::text
{
    namespace
    {
        constexpr char32_t SHORT_STROKE_OVERLAY = 0x0335;
        constexpr char32_t LONG_SOLIDUS_OVERLAY = 0x0338;

        struct SplitLetter
        {
            char32_t letter;
            char32_t base;
            char32_t mark;
        };

        constexpr std::array SPLIT_LETTERS = {
            SplitLetter{.letter = 0x0141, .base = U'L', .mark = SHORT_STROKE_OVERLAY},
            SplitLetter{.letter = 0x0142, .base = U'l', .mark = SHORT_STROKE_OVERLAY},
            SplitLetter{.letter = 0x0110, .base = U'D', .mark = SHORT_STROKE_OVERLAY},
            SplitLetter{.letter = 0x0111, .base = U'd', .mark = SHORT_STROKE_OVERLAY},
            SplitLetter{.letter = 0x00D8, .base = U'O', .mark = LONG_SOLIDUS_OVERLAY},
            SplitLetter{.letter = 0x00F8, .base = U'o', .mark = LONG_SOLIDUS_OVERLAY},
        };

        using GlibString = std::unique_ptr<gchar, decltype(&g_free)>;

        GlibString validUtf8(const std::string_view text)
        {
            return {g_utf8_make_valid(text.empty() ? "" : text.data(), static_cast<gssize>(text.size())), &g_free};
        }

        std::u32string toCodePoints(const gchar* utf8)
        {
            std::u32string codePoints;
            for (const gchar* position = utf8; *position != '\0'; position = g_utf8_next_char(position))
                codePoints.push_back(static_cast<char32_t>(g_utf8_get_char(position)));
            return codePoints;
        }

        const SplitLetter* findSplitLetter(const char32_t codePoint)
        {
            for (const auto& split : SPLIT_LETTERS)
            {
                if (split.letter == codePoint)
                    return &split;
            }
            return nullptr;
        }

        bool lettersLoseMarks(const char32_t codePoint)
        {
            const auto script = g_unichar_get_script(codePoint);
            return script == G_UNICODE_SCRIPT_LATIN || script == G_UNICODE_SCRIPT_GREEK;
        }
    }

    std::u32string decompose(const std::string_view text)
    {
        if (text.empty())
            return {};

        const auto valid = validUtf8(text);
        const GlibString normalized(g_utf8_normalize(valid.get(), -1, G_NORMALIZE_ALL), &g_free);

        std::u32string result;
        for (const auto codePoint : toCodePoints(normalized.get()))
        {
            if (const auto* split = findSplitLetter(codePoint))
            {
                result.push_back(split->base);
                result.push_back(split->mark);
            } else
            {
                result.push_back(codePoint);
            }
        }
        return result;
    }

    std::string toUtf8(const std::u32string_view codePoints)
    {
        std::string result;
        for (const auto codePoint : codePoints)
        {
            std::array<gchar, 6> buffer{};
            const auto length = g_unichar_to_utf8(codePoint, buffer.data());
            result.append(buffer.data(), static_cast<std::size_t>(length));
        }
        return result;
    }

    std::string foldCase(const std::string_view text)
    {
        if (text.empty())
            return {};

        const auto valid = validUtf8(text);
        const GlibString folded(g_utf8_casefold(valid.get(), -1), &g_free);
        return folded.get();
    }

    std::string foldForSearch(const std::string_view text)
    {
        std::u32string result;
        bool marksAreRemoved = false;

        for (const auto codePoint : decompose(foldCase(text)))
        {
            if (!isCombiningMark(codePoint))
            {
                marksAreRemoved = lettersLoseMarks(codePoint);
                result.push_back(codePoint);
            } else if (!marksAreRemoved)
            {
                result.push_back(codePoint);
            }
        }

        // Compatibility forms such as U+210C decompose into upper case letters.
        return foldCase(toUtf8(result));
    }

    bool containsFolded(const std::string_view text, const std::string_view needle)
    {
        return foldForSearch(text).find(foldForSearch(needle)) != std::string::npos;
    }
}
