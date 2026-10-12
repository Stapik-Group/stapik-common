#pragma once

#include "TransformingCloudStorage.hpp"

#include <cstddef>
#include <memory>

namespace stapik::cloud
{
    class CompressedCloudStorage final : public TransformingCloudStorage
    {
    public:
        static constexpr std::size_t DEFAULT_THRESHOLD_BYTES = 32 * 1024;
        static constexpr std::size_t MAX_DECOMPRESSED_BYTES = 256 * 1024 * 1024;

        explicit CompressedCloudStorage(std::unique_ptr<ICloudStorage> inner, std::size_t thresholdBytes = DEFAULT_THRESHOLD_BYTES);

        [[nodiscard]] static bool isCompressed(const nlohmann::json& content);

    private:
        std::size_t m_thresholdBytes;

        [[nodiscard]] nlohmann::json seal(const nlohmann::json& data, const std::string& context) const override;
        [[nodiscard]] CloudDocument open(CloudDocument document, const std::string& context) const override;
    };
}
