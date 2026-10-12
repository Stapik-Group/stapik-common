#pragma once

#include "PrintableDocument.hpp"

#include "stapik/core/Result.hpp"
#include "stapik/storage/FileError.hpp"

#include <filesystem>
#include <string>

namespace stapik::ui
{
    [[nodiscard]] Result<std::string, std::string> renderPdf(const IPrintableDocument& document, const PageSize& pageSize = A4_PORTRAIT);

    // The file is written atomically; a failure is also logged.
    [[nodiscard]] storage::FileResult exportToPdf(const IPrintableDocument& document,
                                                  const std::filesystem::path& path,
                                                  const PageSize& pageSize = A4_PORTRAIT);
}
