#pragma once

#include <filesystem>
#include <string_view>

namespace stapik::storage
{
    inline constexpr auto OWNER_READ_WRITE = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    inline constexpr auto DEFAULT_FILE_PERMISSIONS = OWNER_READ_WRITE |
        std::filesystem::perms::group_read |
        std::filesystem::perms::others_read;

    [[nodiscard]] bool writeFileAtomically(const std::filesystem::path &path, std::string_view content, std::filesystem::perms permissions = DEFAULT_FILE_PERMISSIONS);
}