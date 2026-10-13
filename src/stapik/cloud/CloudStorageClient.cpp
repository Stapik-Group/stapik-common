#include "CloudStorageClient.hpp"
#include "CloudPartitionProtocol.hpp"
#include "CloudStorageException.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/sync/Timestamp.hpp"

#include <format>
#include <glib.h>

CloudStorageClient::CloudStorageClient(CloudStorageConfig config, std::string slotKey, const stapik::http::IHttpClient& httpClient):
    m_config(std::move(config)),
    m_slotKey(std::move(slotKey)),
    m_http(httpClient)
{
    if (m_config.isConfigured() && !m_config.isSecure())
        stapik::log::warning("Cloud API URL does not use https:// - the API key and documents are sent unencrypted. This is not a secure connection.");
}

std::string CloudStorageClient::documentUrl() const
{
    return m_config.normalizedApiUrl() + "/api/v1/documents/" + m_slotKey;
}

stapik::http::Response CloudStorageClient::perform(const stapik::http::Method method, const std::string& url, stapik::http::RequestBody body, const char* failureLabel,
                                                   std::map<std::string, std::string> extraHeaders) const
{
    stapik::http::Request request{
        .method = method,
        .url = url,
        .headers = { { "x-api-key", m_config.apiKey }, { "Content-Type", "application/json" } },
        .body = std::move(body),
        .timeout = TIMEOUT
    };
    request.headers.merge(std::move(extraHeaders));

    auto result = m_http.send(request);
    if (!result)
        throw CloudStorageException(std::string(failureLabel) + result.error().message);

    return std::move(result).value();
}

stapik::http::Response CloudStorageClient::performGet(const std::string& url) const
{
    return perform(stapik::http::Method::Get, url, stapik::http::NoBody{}, "API read failed: ");
}

stapik::http::Response CloudStorageClient::performPut(const std::string& url, const std::string& body, const std::string& ifMatch) const
{
    std::map<std::string, std::string> headers;
    if (!ifMatch.empty())
        headers.emplace("If-Match", ifMatch);

    return perform(stapik::http::Method::Put, url, stapik::http::toBytes(body), "API write failed: ", std::move(headers));
}

stapik::http::Response CloudStorageClient::performDelete(const std::string& url) const
{
    return perform(stapik::http::Method::Delete, url, stapik::http::NoBody{}, "API delete failed: ");
}

auto CloudStorageClient::knownVersion(const std::string& url) const -> std::optional<KnownVersion>
{
    const std::scoped_lock lock(m_knownVersionsMutex);

    if (const auto found = m_knownVersions.find(url); found != m_knownVersions.end())
        return found->second;

    return std::nullopt;
}

void CloudStorageClient::remember(const std::string& url, const CloudDocument& document) const
{
    const std::scoped_lock lock(m_knownVersionsMutex);
    m_knownVersions.insert_or_assign(url, KnownVersion{ .updatedAt = document.updatedAt, .etag = document.etag });
}

void CloudStorageClient::forget(const std::string& url) const
{
    const std::scoped_lock lock(m_knownVersionsMutex);
    m_knownVersions.erase(url);
}

CloudDocument CloudStorageClient::parseDocumentResponse(const stapik::http::Response& response)
{
    const auto json = nlohmann::json::parse(response.body.begin(), response.body.end());
    const auto contentString = json.at("content").get<std::string>();
    const auto updatedAtString = json.at("updatedAt").get<std::string>();

    nlohmann::json content = nlohmann::json::object();
    if (!contentString.empty())
        content = nlohmann::json::parse(contentString);

    return CloudDocument{ .content = content, .updatedAt = parseTimestamp(updatedAtString), .etag = response.header("etag").value_or("") };
}

std::optional<CloudDocument> CloudStorageClient::readDocument(const std::string& url) const
{
    const auto response = performGet(url);

    if (response.status == 404)
    {
        forget(url);
        return std::nullopt;
    }

    if (response.status != 200)
        throw CloudStorageException(std::format("API read failed with status {}", response.status));
    try
    {
        auto document = parseDocumentResponse(response);
        remember(url, document);
        return document;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
    }
}

CloudWriteResult CloudStorageClient::writeDocument(const std::string& url, const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
{
    // The server detects conflicts by ETag (If-Match), while the sync remembers the updatedAt of the last version it knew.
    // The ETag of that version is either already known from an earlier read or write, or the current version is read first:
    // its ETag is the one that matches the baseline, or the baseline is stale.
    std::string ifMatch;
    if (const auto known = knownVersion(url); known && known->updatedAt == clientLastKnownUpdate && !known->etag.empty())
    {
        ifMatch = known->etag;
    }
    else if (const auto current = readDocument(url))
    {
        if (current->updatedAt != clientLastKnownUpdate)
            return CloudWriteResult{ .document = *current, .conflict = true };

        ifMatch = current->etag;
    }

    const nlohmann::json payload = { { "content", data.dump() } };

    const auto response = performPut(url, payload.dump(), ifMatch);

    if (response.status != 200 && response.status != 409)
        throw CloudStorageException(std::format("API write failed with status {}", response.status));

    try
    {
        CloudWriteResult result{
            .document = parseDocumentResponse(response),
            .conflict = response.status == 409
        };
        remember(url, result.document);
        return result;
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

    const auto [httpStatus, body, retryAfter, headers] = performGet(stapik::cloud::partitionsUrl(documentUrl()));

    if (httpStatus == 404)
        return {};

    if (httpStatus != 200)
        throw CloudStorageException(std::format("API partition list failed with status {}", httpStatus));

    return stapik::cloud::parsePartitionList(stapik::http::toText(body));
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

    const auto url = stapik::cloud::partitionUrl(documentUrl(), partition);
    const auto [httpStatus, body, retryAfter, headers] = performDelete(url);

    if (httpStatus == 204 || httpStatus == 404)
        forget(url);

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
