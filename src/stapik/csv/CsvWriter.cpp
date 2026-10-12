#include "stapik/csv/CsvWriter.hpp"

#include "stapik/domain/MoneyFormatter.hpp"
#include "stapik/storage/AtomicFile.hpp"

#include <algorithm>

namespace stapik::csv
{
    namespace
    {
        constexpr std::string_view BYTE_ORDER_MARK = "\xEF\xBB\xBF";
        constexpr std::string_view RECORD_END = "\r\n";
        constexpr char QUOTE = '"';

        char separatorCharacter(const CsvSeparator separator)
        {
            return separator == CsvSeparator::Semicolon ? ';' : ',';
        }

        bool containsSpecialCharacter(const std::string_view field, const char separator)
        {
            return std::ranges::any_of(field, [separator](const char character)
            {
                return character == separator || character == QUOTE || character == '\r' || character == '\n';
            });
        }

        bool mustBeQuoted(const std::string_view field, const char separator, const bool isOnlyFieldOfRow)
        {
            const bool wouldLookLikeBlankLine = isOnlyFieldOfRow && field.empty();
            return wouldLookLikeBlankLine || containsSpecialCharacter(field, separator);
        }

        void appendQuoted(std::string& output, const std::string_view field)
        {
            output += QUOTE;
            for (const char character : field)
            {
                if (character == QUOTE)
                    output += QUOTE;
                output += character;
            }
            output += QUOTE;
        }
    }

    CsvOptions CsvOptions::forExcel(const std::string_view languageCode)
    {
        const bool usesDecimalComma = domain::numberFormatFor(languageCode).decimalSeparator == ",";
        return {
            .separator = usesDecimalComma ? CsvSeparator::Semicolon : CsvSeparator::Comma,
            .byteOrderMark = true
        };
    }

    CsvWriter::CsvWriter(const CsvOptions options) :
        m_options(options)
    {
        if (m_options.byteOrderMark)
            m_content = BYTE_ORDER_MARK;
    }

    void CsvWriter::addRow(const CsvRow& fields)
    {
        const char separator = separatorCharacter(m_options.separator);
        const bool isOnlyFieldOfRow = fields.size() == 1;

        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            if (index > 0)
                m_content += separator;

            if (mustBeQuoted(fields[index], separator, isOnlyFieldOfRow))
                appendQuoted(m_content, fields[index]);
            else
                m_content += fields[index];
        }

        m_content += RECORD_END;
    }

    const std::string& CsvWriter::content() const
    {
        return m_content;
    }

    storage::FileResult CsvWriter::saveTo(const std::filesystem::path& path) const
    {
        return storage::writeFileAtomically(path, m_content);
    }
}
