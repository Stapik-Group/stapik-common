#pragma once

#include "ICloudStorage.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    class TransformingCloudStorage : public ICloudStorage
    {
    public:
        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override;
        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;

        [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override;
        [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override;
        [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;
        [[nodiscard]] bool deletePartition(const std::string& partition) const override;

        [[nodiscard]] CloudChangeList listChanges(std::chrono::system_clock::time_point since) const override;

    protected:
        explicit TransformingCloudStorage(std::unique_ptr<ICloudStorage> inner);

        [[nodiscard]] virtual nlohmann::json seal(const nlohmann::json& data, const std::string& context) const = 0;
        [[nodiscard]] virtual CloudDocument open(CloudDocument document, const std::string& context) const = 0;

    private:
        std::unique_ptr<ICloudStorage> m_inner;

        [[nodiscard]] static std::string partitionContext(const std::string& partition);
        [[nodiscard]] std::optional<CloudDocument> openIfPresent(std::optional<CloudDocument> document, const std::string& context) const;
        [[nodiscard]] CloudWriteResult openWriteResult(CloudWriteResult result, const nlohmann::json& sentData, const std::string& context) const;
    };
}
