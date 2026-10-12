#pragma once

#include "Tutorial.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace stapik::tutorial
{
    inline constexpr std::string_view FALLBACK_LANGUAGE = "en";
    inline constexpr std::string_view BOOK_PAGE_FILE = "index.md";
    inline constexpr std::string_view CHAPTER_EXTENSION = ".md";

    [[nodiscard]] std::optional<std::filesystem::path> findLanguageDirectory(const std::filesystem::path& tutorialDirectory, std::string_view languageCode);
    [[nodiscard]] Tutorial loadTutorial(const std::filesystem::path& tutorialDirectory, std::string_view languageCode);
}
