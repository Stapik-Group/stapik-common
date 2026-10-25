#pragma once

#include <filesystem>
#include <format>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace stapik::test
{
    inline std::filesystem::path createTemporaryDirectory(const std::string_view prefix)
    {
        constexpr int MAX_ATTEMPTS = 100;
        static std::mt19937_64 generator{ std::random_device{}() };

        for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
        {
            const auto name = std::format("{}-{:016x}", prefix, generator());
            const auto candidate = std::filesystem::temp_directory_path() / name;

            if (std::error_code errorCode; std::filesystem::create_directory(candidate, errorCode))
                return candidate;
        }

        throw std::runtime_error("Cannot create a temporary directory for " + std::string(prefix));
    }

    class TemporaryDirectory
    {
    public:
        explicit TemporaryDirectory(const std::string_view prefix = "stapik-test") :
            m_path(createTemporaryDirectory(prefix))
        {}

        ~TemporaryDirectory()
        {
            std::error_code errorCode;
            std::filesystem::remove_all(m_path, errorCode);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    private:
        std::filesystem::path m_path;
    };
}
