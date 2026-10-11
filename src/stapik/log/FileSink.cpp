#include "FileSink.hpp"

#include <chrono>
#include <format>
#include <string>
#include <system_error>
#include <utility>

namespace stapik::log
{
    FileSink::FileSink(std::filesystem::path file, const FileRotation rotation, const time::IClock& clock) :
        m_file(std::move(file)),
        m_rotation(rotation)
    {
        open();

        const auto now = std::chrono::floor<std::chrono::seconds>(clock.now());
        writeLine(std::format("---- started {:%F %T} UTC ----", now));
    }

    void FileSink::write(const LogRecord& record)
    {
        if (m_size >= m_rotation.maxFileSize)
        {
            m_stream.close();
            open();
        }

        writeLine(formatRecord(record));
    }

    void FileSink::open()
    {
        rotateLogFiles(m_file, m_rotation);

        m_stream.clear();
        m_stream.open(m_file, std::ios::binary | std::ios::app);

        std::error_code errorCode;
        const auto existingSize = std::filesystem::file_size(m_file, errorCode);
        m_size = errorCode ? 0 : existingSize;
    }

    void FileSink::writeLine(const std::string& line)
    {
        if (!m_stream.is_open())
            return;

        m_stream << line << '\n';
        m_stream.flush();
        m_size += line.size() + 1;
    }
}
