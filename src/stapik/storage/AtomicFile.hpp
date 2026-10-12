#pragma once

#include "FileError.hpp"

#include <filesystem>
#include <string_view>

namespace stapik::storage
{
    inline constexpr auto OWNER_READ_WRITE = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    inline constexpr auto DEFAULT_FILE_PERMISSIONS = OWNER_READ_WRITE |
        std::filesystem::perms::group_read |
        std::filesystem::perms::others_read;

    // The failure is also logged. Either the target is replaced completely or it is left as it was.
    [[nodiscard]] FileResult writeFileAtomically(const std::filesystem::path &path, std::string_view content, std::filesystem::perms permissions = DEFAULT_FILE_PERMISSIONS);
}