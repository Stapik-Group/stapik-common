#pragma once

#include "CloudStorageConfig.hpp"
#include "ICloudStorage.hpp"

#include "ApiKeyExpiry.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

class CloudStorageClient : public ICloudStorage
{
public:
    CloudStorageClient(CloudStorageConfig config, std::string slotKey, const stapik::http::IHttpClient& httpClient = stapik::http::defaultHttpClient(),
                       std::shared_ptr<stapik::cloud::ApiKeyExpiryMonitor> expiryMonitor = {});

    [[nodiscard]] std::optional<CloudDocument> loadDocument() const override;
    [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;

    [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override;
    [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override;
    [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const override;
    [[nodiscard]] bool deletePartition(const std::string& partition) const override;

private:
    static constexpr std::chrono::seconds TIMEOUT{ 8 };

    // The version of a document (or partition) this client last saw on the server, kept per URL.
    // A save whose baseline equals it can send the ETag at once instead of reading the document first.
    struct KnownVersion
    {
        std::chrono::system_clock::time_point updatedAt;
        std::string etag;
    };

    CloudStorageConfig m_config;
    std::string m_slotKey;
    const stapik::http::IHttpClient& m_http;
    std::shared_ptr<stapik::cloud::ApiKeyExpiryMonitor> m_expiryMonitor;
    mutable std::mutex m_knownVersionsMutex;
    mutable std::map<std::string, KnownVersion> m_knownVersions;

    [[nodiscard]] stapik::http::Response perform(stapik::http::Method method, const std::string& url, stapik::http::RequestBody body, const char* failureLabel,
                                                 std::map<std::string, std::string> extraHeaders = {}) const;
    [[nodiscard]] stapik::http::Response performGet(const std::string& url) const;
    [[nodiscard]] stapik::http::Response performPut(const std::string& url, const std::string& body, const std::string& ifMatch) const;
    [[nodiscard]] stapik::http::Response performDelete(const std::string& url) const;
    [[nodiscard]] std::string documentUrl() const;

    [[nodiscard]] std::optional<KnownVersion> knownVersion(const std::string& url) const;
    void remember(const std::string& url, const CloudDocument& document) const;
    void forget(const std::string& url) const;

    [[nodiscard]] std::optional<CloudDocument> readDocument(const std::string& url) const;
    [[nodiscard]] CloudWriteResult writeDocument(const std::string& url, const nlohmann::json& data, std::chrono::system_clock::time_point clientLastKnownUpdate) const;

    static CloudDocument parseDocumentResponse(const stapik::http::Response& response);
    static std::chrono::system_clock::time_point parseTimestamp(const std::string& text);
};
