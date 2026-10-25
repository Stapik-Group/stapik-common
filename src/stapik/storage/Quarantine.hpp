#pragma once

#include "FileError.hpp"

#include "stapik/core/Result.hpp"
#include "stapik/time/Clock.hpp"

#include <filesystem>

namespace stapik::storage
{
    using QuarantineResult = Result<std::filesystem::path, FileError>;

    [[nodiscard]] QuarantineResult quarantineFile(const std::filesystem::path& file, time::TimePoint now);
}
