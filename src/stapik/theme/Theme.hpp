#pragma once
#include <string>

enum class Theme { Classic, Modern, ClassicPink };

std::string themeToFileString(Theme theme);
Theme themeFromFileString(const std::string& value);