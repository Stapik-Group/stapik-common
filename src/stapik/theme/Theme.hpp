#pragma once
#include <string>

enum class Theme { Classic, Modern };

std::string toFileString(Theme theme);
Theme fromFileString(const std::string& value);