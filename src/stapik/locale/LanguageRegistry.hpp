#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::locale
{
    struct LanguageInfo
    {
        std::string code;
        std::string displayName;
        std::vector<std::filesystem::path> files;
    };

    class LanguageRegistry
    {
    public:
        LanguageRegistry() = default;
        explicit LanguageRegistry(const std::filesystem::path& directory);
        void addDirectory(const std::filesystem::path& directory);
        [[nodiscard]] const std::vector<LanguageInfo>& languages() const;
        [[nodiscard]] const LanguageInfo* find(std::string_view code) const;
        [[nodiscard]] bool contains(std::string_view code) const;
    private:
        std::vector<LanguageInfo> m_languages;
    };
}
