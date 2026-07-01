#pragma once

#include <string>

enum class Locale
{
    PL,
    EN,
    DE
};

inline const char *toString(const Locale locale)
{
    switch (locale)
    {
        using enum Locale;
        case PL: return "PL";
        case EN: return "EN";
        case DE: return "DE";
        default: return "[Unknown Locale]";
    }
}

inline const char *toFileString(const Locale locale)
{
    switch (locale)
    {
        using enum Locale;
        case PL: return "pl";
        case EN: return "en";
        case DE: return "de";
        default: return "en";
    }
}

inline Locale fromFileString(const std::string &str)
{
    using enum Locale;
    if (str == "pl") return PL;
    if (str == "de") return DE;
    return EN;
}
