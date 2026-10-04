#pragma once
#include <string>

enum class Theme { Classic, Modern, ClassicPink, Dark };

std::string themeToFileString(Theme theme);
Theme themeFromFileString(const std::string& value);