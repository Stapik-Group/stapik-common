#pragma once

#include "stapik/core/Result.hpp"
#include "stapik/storage/PathText.hpp"

#include <filesystem>
#include <format>
#include <string>
#include <system_error>

namespace stapik::storage
{
    struct FileError
    {
        std::filesystem::path path;
        // The operation that failed, for example "rename".
        std::string step;
        std::error_code code;

        [[nodiscard]] std::string message() const
        {
            return std::format("{} failed for {}: {}", step, pathText(path), code.message());
        }
    };

    using FileResult = Result<void, FileError>;
}
