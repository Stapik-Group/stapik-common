#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::theme
{
    struct ThemeInfo
    {
        std::string id;
        std::string nameKey;
        std::vector<std::filesystem::path> files;
    };

    class ThemeRegistry
    {
    public:
        ThemeRegistry() = default;
        void addDirectory(const std::filesystem::path& resourcesDir);
        [[nodiscard]] const std::vector<ThemeInfo>& themes() const;
        [[nodiscard]] const ThemeInfo* find(std::string_view id) const;
        [[nodiscard]] bool contains(std::string_view id) const;
    private:
        ThemeInfo& findOrCreate(const std::string& id);
        std::vector<ThemeInfo> m_themes;
    };
}
