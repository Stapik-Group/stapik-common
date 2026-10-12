#pragma once

#include "stapik/core/Result.hpp"

#include <string>
#include <utility>

namespace stapik::archive
{
    enum class ArchiveErrorKind
    {
        Io,
        NotAnArchive,
        Corrupted,
        Unsupported,
        WrongApplication,
        Cloud
    };

    struct ArchiveError
    {
        ArchiveErrorKind kind = ArchiveErrorKind::Io;
        std::string detail;

        [[nodiscard]] const std::string& message() const
        {
            return detail;
        }
    };

    template<typename ValueType>
    using ArchiveResult = Result<ValueType, ArchiveError>;

    [[nodiscard]] inline Failure<ArchiveError> archiveFailure(const ArchiveErrorKind kind, std::string detail)
    {
        return Failure(ArchiveError{ .kind = kind, .detail = std::move(detail) });
    }
}
