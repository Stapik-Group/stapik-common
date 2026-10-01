#include "ThemeRegistry.hpp"

#include "stapik/log/Log.hpp"

#include <algorithm>
#include <cctype>

namespace stapik::theme
{
    namespace
    {
        constexpr std::string_view LEGACY_PREFIX = "style-";

        std::string lowercase(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return text;
        }

        std::vector<std::filesystem::path> cssFilesIn(const std::filesystem::path& directory)
        {
            std::vector<std::filesystem::path> files;

            std::error_code errorCode;
            if (!std::filesystem::is_directory(directory, errorCode))
                return files;

            for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode))
            {
                if (const auto& path = entry.path(); entry.is_regular_file(errorCode) && path.extension() == ".css" && !path.filename().string().starts_with('.'))
                    files.push_back(path);
            }

            std::ranges::sort(files);
            return files;
        }
    }

    void ThemeRegistry::addDirectory(const std::filesystem::path& resourcesDir)
    {
        if (std::error_code errorCode; !std::filesystem::is_directory(resourcesDir, errorCode))
        {
            log::warning("Resources directory not found: {}", resourcesDir.string());
            return;
        }

        for (const auto& file : cssFilesIn(resourcesDir / "themes"))
            findOrCreate(lowercase(file.stem().string())).files.push_back(file);

        for (const auto& file : cssFilesIn(resourcesDir))
        {
            const auto name = lowercase(file.stem().string());
            if (!name.starts_with(LEGACY_PREFIX) || name.size() == LEGACY_PREFIX.size())
                continue;

            findOrCreate(name.substr(LEGACY_PREFIX.size())).files.push_back(file);
        }

        std::ranges::sort(m_themes, {}, &ThemeInfo::id);
    }

    ThemeInfo& ThemeRegistry::findOrCreate(const std::string& id)
    {
        if (const auto theme = std::ranges::find(m_themes, id, &ThemeInfo::id); theme != m_themes.end())
            return *theme;

        m_themes.push_back(ThemeInfo{ .id = id, .nameKey = "theme." + id, .files = {}});
        return m_themes.back();
    }

    const std::vector<ThemeInfo>& ThemeRegistry::themes() const
    {
        return m_themes;
    }

    const ThemeInfo* ThemeRegistry::find(const std::string_view id) const
    {
        const auto theme = std::ranges::find_if(m_themes, [id](const ThemeInfo& info)
        {
            return info.id == id;
        });

        return theme == m_themes.end() ? nullptr : &*theme;
    }

    bool ThemeRegistry::contains(const std::string_view id) const
    {
        return find(id) != nullptr;
    }
}
