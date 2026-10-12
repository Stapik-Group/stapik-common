#pragma once

#include "ICloudStorage.hpp"

#include "stapik/crypto/PayloadCipher.hpp"

#include <memory>
#include <string>

namespace stapik::cloud
{
    class EncryptedCloudStorage final : public ICloudStorage
    {
    public:
        EncryptedCloudStorage(std::unique_ptr<ICloudStorage> inner, std::string passphrase, std::string scope);

        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override;
        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;

        [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override;
        [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override;
        [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;
        [[nodiscard]] bool deletePartition(const std::string& partition) const override;

    private:
        std::unique_ptr<ICloudStorage> m_inner;
        crypto::PayloadCipher m_cipher;
        std::string m_scope;

        [[nodiscard]] std::string documentContext() const;
        [[nodiscard]] std::string partitionContext(const std::string& partition) const;
        [[nodiscard]] nlohmann::json seal(const nlohmann::json& data, const std::string& context) const;
        [[nodiscard]] CloudDocument open(CloudDocument document, const std::string& context) const;
        [[nodiscard]] std::optional<CloudDocument> openIfPresent(std::optional<CloudDocument> document, const std::string& context) const;
        [[nodiscard]] CloudWriteResult write(CloudWriteResult result, const nlohmann::json& sentData, const std::string& context) const;
    };
}
