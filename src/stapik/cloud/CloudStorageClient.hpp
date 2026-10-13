#pragma once

#include "CloudStorageConfig.hpp"
#include "ICloudStorage.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <map>
#include <optional>
#include <vector>

class CloudStorageClient : public ICloudStorage
{
public:
    CloudStorageClient(CloudStorageConfig config, std::string slotKey, const stapik::http::IHttpClient& httpClient = stapik::http::defaultHttpClient());

    [[nodiscard]] std::optional<CloudDocument> loadDocument() const override;
    [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;

    [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override;
    [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override;
    [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;
    [[nodiscard]] bool deletePartition(const std::string& partition) const override;

private:
    static constexpr std::chrono::seconds TIMEOUT{ 8 };

    CloudStorageConfig m_config;
    std::string m_slotKey;
    const stapik::http::IHttpClient& m_http;

    [[nodiscard]] stapik::http::Response perform(stapik::http::Method method, const std::string& url, stapik::http::RequestBody body, const char* failureLabel,
                                                 std::map<std::string, std::string> extraHeaders = {}) const;
    [[nodiscard]] stapik::http::Response performGet(const std::string& url) const;
    [[nodiscard]] stapik::http::Response performPut(const std::string& url, const std::string& body, const std::string& ifMatch) const;
    [[nodiscard]] stapik::http::Response performDelete(const std::string& url) const;
    [[nodiscard]] std::string documentUrl() const;

    [[nodiscard]] std::optional<CloudDocument> readDocument(const std::string& url) const;
    [[nodiscard]] CloudWriteResult writeDocument(const std::string& url, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const;

    static CloudDocument parseDocumentResponse(const stapik::http::Response& response);
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& text);
};
