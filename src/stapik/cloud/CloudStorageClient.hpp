#pragma once

#include "CloudStorageConfig.hpp"
#include <nlohmann/json.hpp>
#include <string>

struct curl_slist;

class CloudStorageClient
{
public:
    CloudStorageClient(CloudStorageConfig config, std::string filename);

    [[nodiscard]] nlohmann::json loadJson() const;
    void saveJson(const nlohmann::json& data) const;
private:
    static constexpr long TIMEOUT_SECONDS = 8L;

    CloudStorageConfig m_config;
    std::string m_filename;

    [[nodiscard]] std::string fetchRawContent() const;
    void pushRawContent(const std::string& content) const;
    [[nodiscard]] curl_slist* buildHeaders() const;

    static size_t writeCallback(const char* ptr, size_t size, size_t nmemb, std::string* response);
};