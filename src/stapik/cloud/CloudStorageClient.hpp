#pragma once

#include "CloudStorageConfig.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <optional>

struct curl_slist;

struct CloudDocument
{
    nlohmann::json content;
    std::chrono::system_clock::time_point updatedAt;
};

struct CloudWriteResult
{
    CloudDocument document;
    bool conflict{};
};

class CloudStorageClient
{
public:
    CloudStorageClient(CloudStorageConfig config, std::string slotKey);

    [[nodiscard]] std::optional<CloudDocument> loadDocument() const;

    [[nodiscard]] CloudWriteResult saveDocument(
        const nlohmann::json& data,
        std::chrono::system_clock::time_point clientLastKnownUpdate) const;

private:
    static constexpr long TIMEOUT_SECONDS = 8L;

    CloudStorageConfig m_config;
    std::string m_slotKey;

    struct RawResponse
    {
        long httpStatus;
        std::string body;
    };

    [[nodiscard]] RawResponse performGet() const;
    [[nodiscard]] RawResponse performPut(const std::string& body) const;
    [[nodiscard]] curl_slist* buildHeaders() const;
    [[nodiscard]] std::string documentUrl() const;

    static CloudDocument parseDocumentResponse(const std::string& body);
    static std::string formatIso8601(std::chrono::system_clock::time_point tp);
    static std::chrono::system_clock::time_point parseIso8601(const std::string& str);

    static size_t writeCallback(const char* ptr, size_t size, size_t nmemb, std::string* response);
};