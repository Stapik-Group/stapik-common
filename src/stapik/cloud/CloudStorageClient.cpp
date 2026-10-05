#include "CloudStorageClient.hpp"
#include "CloudPartitionProtocol.hpp"
#include "CloudStorageException.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/sync/Timestamp.hpp"

#include <curl/curl.h>
#include <ctime>
#include <filesystem>
#include <format>
#include <glib.h>

namespace
{
    void applySecurityOptions(CURL* curl)
    {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

#ifdef _WIN32
        // The CA bundle path compiled into libcurl points into the build machine's MSYS2 tree, which does not exist
        // on the user's PC. Trust the Windows certificate store, plus the bundle shipped next to the executable.
#if LIBCURL_VERSION_NUM >= 0x074700
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
        std::error_code errorCode;
        if (const auto bundle = AppPaths::executableDir() / "etc" / "ssl" / "certs" / "ca-bundle.crt"; std::filesystem::is_regular_file(bundle, errorCode))
            curl_easy_setopt(curl, CURLOPT_CAINFO, stapik::storage::pathText(bundle).c_str());
#endif
    }
}

CloudStorageClient::CloudStorageClient(CloudStorageConfig config, std::string slotKey):
    m_config(std::move(config)),
    m_slotKey(std::move(slotKey))
{
    if (m_config.isConfigured() && !m_config.isSecure())
        stapik::log::warning("Cloud API URL does not use https:// - the API key and documents are sent unencrypted. This is not a secure connection.");
}

std::string CloudStorageClient::documentUrl() const
{
    return m_config.normalizedApiUrl() + "/api/v1/documents/" + m_slotKey;
}

curl_slist* CloudStorageClient::buildHeaders() const
{
    const auto authHeader = std::string("x-api-key: ") + m_config.apiKey;
    curl_slist* headers = curl_slist_append(nullptr, authHeader.c_str());
    return curl_slist_append(headers, "Content-Type: application/json");
}

CloudStorageClient::RawResponse CloudStorageClient::perform(const std::string& url, const char* method, const std::string* body, const char* failureLabel) const
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
        throw CloudStorageException("Cannot initialize CURL!");

    std::string response;
    curl_slist* headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (method != nullptr)
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (body != nullptr)
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    applySecurityOptions(curl);

    const auto result = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
        throw CloudStorageException(std::string(failureLabel) + curl_easy_strerror(result));

    return { .httpStatus = httpStatus, .body = response };
}

CloudStorageClient::RawResponse CloudStorageClient::performGet(const std::string& url) const
{
    return perform(url, nullptr, nullptr, "API read failed: ");
}

CloudStorageClient::RawResponse CloudStorageClient::performPut(const std::string& url, const std::string& body) const
{
    return perform(url, "PUT", &body, "API write failed: ");
}

CloudStorageClient::RawResponse CloudStorageClient::performDelete(const std::string& url) const
{
    return perform(url, "DELETE", nullptr, "API delete failed: ");
}

CloudDocument CloudStorageClient::parseDocumentResponse(const std::string& body)
{
    const auto json = nlohmann::json::parse(body);
    const auto contentString = json.at("content").get<std::string>();
    const auto updatedAtString = json.at("updatedAt").get<std::string>();

    nlohmann::json content = nlohmann::json::object();
    if (!contentString.empty())
        content = nlohmann::json::parse(contentString);

    return CloudDocument{ .content = content, .updatedAt = parseTimestamp(updatedAtString) };
}

std::optional<CloudDocument> CloudStorageClient::readDocument(const std::string& url) const
{
    const auto [httpStatus, body] = performGet(url);

    if (httpStatus == 404)
        return std::nullopt;

    if (httpStatus != 200)
        throw CloudStorageException(std::format("API read failed with status {}", httpStatus));
    try
    {
        return parseDocumentResponse(body);
    }
    catch (const nlohmann::json::exception& e)
    {
        throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
    }
}

CloudWriteResult CloudStorageClient::writeDocument(const std::string& url, const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
{
    const nlohmann::json payload = {
        { "content", data.dump() },
        { "clientLastKnownUpdate", stapik::sync::toIso8601(clientLastKnownUpdate, stapik::sync::TimestampPrecision::Microseconds) }
    };

    const auto [httpStatus, body] = performPut(url, payload.dump());

    if (httpStatus != 200 && httpStatus != 409)
        throw CloudStorageException(std::format("API write failed with status {}", httpStatus));

    try
    {
        return CloudWriteResult{
            .document = parseDocumentResponse(body),
            .conflict = httpStatus == 409
        };
    }
    catch (const nlohmann::json::exception& e)
    {
        throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
    }
}

std::optional<CloudDocument> CloudStorageClient::loadDocument() const
{
    if (!m_config.isConfigured())
        return std::nullopt;

    return readDocument(documentUrl());
}

CloudWriteResult CloudStorageClient::saveDocument(const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
{
    if (!m_config.isConfigured())
    {
        stapik::log::debug("saveDocument: not configured (apiUrl='{}', apiKey empty: {})", m_config.apiUrl, m_config.apiKey.empty());
        throw CloudStorageException("Cloud sync is not configured");
    }

    return writeDocument(documentUrl(), data, clientLastKnownUpdate);
}

std::vector<CloudPartitionInfo> CloudStorageClient::listPartitions() const
{
    if (!m_config.isConfigured())
        return {};

    const auto [httpStatus, body] = performGet(stapik::cloud::partitionsUrl(documentUrl()));

    if (httpStatus == 404)
        return {};

    if (httpStatus != 200)
        throw CloudStorageException(std::format("API partition list failed with status {}", httpStatus));

    return stapik::cloud::parsePartitionList(body);
}

std::optional<CloudDocument> CloudStorageClient::loadPartition(const std::string& partition) const
{
    if (!m_config.isConfigured())
        return std::nullopt;

    return readDocument(stapik::cloud::partitionUrl(documentUrl(), partition));
}

CloudWriteResult CloudStorageClient::savePartition(const std::string& partition, const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
{
    if (!m_config.isConfigured())
        throw CloudStorageException("Cloud sync is not configured");

    return writeDocument(stapik::cloud::partitionUrl(documentUrl(), partition), data, clientLastKnownUpdate);
}

bool CloudStorageClient::deletePartition(const std::string& partition) const
{
    if (!m_config.isConfigured())
        return false;

    const auto [httpStatus, body] = performDelete(stapik::cloud::partitionUrl(documentUrl(), partition));

    if (httpStatus == 204)
        return true;

    if (httpStatus == 404)
        return false;

    throw CloudStorageException(std::format("API partition delete failed with status {}", httpStatus));
}

std::chrono::system_clock::time_point CloudStorageClient::parseTimestamp(const std::string& text)
{
    const auto timestamp = stapik::sync::parseIso8601(text);
    if (!timestamp)
        throw CloudStorageException("Invalid ISO-8601 timestamp: " + text);

    return *timestamp;
}

size_t CloudStorageClient::writeCallback(const char* ptr, const size_t size, const size_t nmemb, std::string* response)
{
    const size_t totalSize = size * nmemb;
    response->append(ptr, totalSize);
    return totalSize;
}