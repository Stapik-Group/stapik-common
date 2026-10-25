#include "Quarantine.hpp"

#include <chrono>
#include <format>
#include <system_error>

namespace stapik::storage
{
    QuarantineResult quarantineFile(const std::filesystem::path& file, const time::TimePoint now)
    {
        const auto stamp = std::format("{:%Y%m%d-%H%M%S}", std::chrono::floor<std::chrono::seconds>(now));

        std::error_code errorCode;
        auto target = file;
        target += std::format(".corrupt-{}", stamp);

        int suffix = 2;
        while (std::filesystem::exists(target, errorCode))
        {
            target = file;
            target += std::format(".corrupt-{}-{}", stamp, suffix++);
        }

        std::filesystem::rename(file, target, errorCode);
        if (errorCode)
            return Failure(FileError{ .path = file, .step = "move aside", .code = errorCode });

        return target;
    }
}
