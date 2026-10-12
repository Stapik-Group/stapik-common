#pragma once

#include "ILogSink.hpp"
#include "LogFile.hpp"

#include "stapik/time/Clock.hpp"

#include <filesystem>
#include <fstream>

namespace stapik::log
{
    // Appends records to a file and rotates it while the application runs. A file that cannot be opened is skipped
    // silently: a sink must not log, and a broken log file must not stop the application.
    class FileSink final : public ILogSink
    {
    public:
        explicit FileSink(std::filesystem::path file, FileRotation rotation = DEFAULT_FILE_ROTATION,
                          const time::IClock& clock = time::systemClock());

        void write(const LogRecord& record) override;

    private:
        void open();
        void writeLine(const std::string& line);

        std::filesystem::path m_file;
        FileRotation m_rotation;
        std::ofstream m_stream;
        std::uintmax_t m_size = 0;
    };
}
