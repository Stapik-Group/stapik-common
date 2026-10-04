#pragma once

#include "CloudStorageConfig.hpp"
#include "ICloudStorage.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <optional>
#include <vector>

struct curl_slist;

class CloudStorageClient : public ICloudStorage
{
public:
    CloudStorageClient(CloudStorageConfig config, std::string slotKey);

    [[nodiscard]] std::optional<CloudDocument> loadDocument() const override;
    [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;

    [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override;
    [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override;
    [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;
    bool deletePartition(const std::string& partition) const override;

private:
    static constexpr long TIMEOUT_SECONDS = 8L;

    CloudStorageConfig m_config;
    std::string m_slotKey;

    struct RawResponse
    {
        long httpStatus;
        std::string body;
    };

    [[nodiscard]] RawResponse perform(const std::string& url, const char* method, const std::string* body, const char* failureLabel) const;
    [[nodiscard]] RawResponse performGet(const std::string& url) const;
    [[nodiscard]] RawResponse performPut(const std::string& url, const std::string& body) const;
    [[nodiscard]] RawResponse performDelete(const std::string& url) const;
    [[nodiscard]] curl_slist* buildHeaders() const;
    [[nodiscard]] std::string documentUrl() const;

    [[nodiscard]] std::optional<CloudDocument> readDocument(const std::string& url) const;
    [[nodiscard]] CloudWriteResult writeDocument(const std::string& url, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const;

    static CloudDocument parseDocumentResponse(const std::string& body);
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& text);

    static size_t writeCallback(const char* ptr, size_t size, size_t nmemb, std::string* response);
};