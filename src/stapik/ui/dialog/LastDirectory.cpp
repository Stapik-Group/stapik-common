#include "LastDirectory.hpp"

#include "stapik/storage/PathText.hpp"

namespace
{
    std::filesystem::path directoryOf(const std::filesystem::path& chosen)
    {
        std::error_code errorCode;
        return std::filesystem::is_directory(chosen, errorCode) ? chosen : chosen.parent_path();
    }
}

namespace stapik::ui
{
    std::optional<std::filesystem::path> loadLastDirectory(const settings::SettingsStore& store, const std::string& key)
    {
        const auto text = store.get<std::string>(key, {});
        if (text.empty())
            return std::nullopt;

        auto directory = storage::pathFromText(text);
        if (std::error_code errorCode; !std::filesystem::is_directory(directory, errorCode))
            return std::nullopt;

        return directory;
    }

    storage::FileResult saveLastDirectory(settings::SettingsStore& store, const std::string& key, const std::filesystem::path& chosen)
    {
        const auto directory = directoryOf(chosen);
        if (directory.empty())
            return {};

        store.set<std::string>(key, storage::pathText(directory));
        return store.save();
    }
}
