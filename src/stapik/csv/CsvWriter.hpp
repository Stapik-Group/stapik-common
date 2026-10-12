#pragma once

#include "stapik/storage/FileError.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::csv
{
    enum class CsvSeparator
    {
        Comma,
        Semicolon
    };

    struct CsvOptions
    {
        CsvSeparator separator = CsvSeparator::Comma;
        bool byteOrderMark = false;

        [[nodiscard]] static CsvOptions forExcel(std::string_view languageCode);
    };

    using CsvRow = std::vector<std::string>;

    class CsvWriter
    {
    public:
        explicit CsvWriter(CsvOptions options = {});

        void addRow(const CsvRow& fields);

        [[nodiscard]] const std::string& content() const;
        [[nodiscard]] storage::FileResult saveTo(const std::filesystem::path& path) const;

    private:
        CsvOptions m_options;
        std::string m_content;
    };
}
