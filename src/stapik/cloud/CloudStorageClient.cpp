#include "CloudStorageClient.hpp"
#include "CloudStorageException.hpp"

#include <curl/curl.h>
#include <ctime>
#include <glib.h>

CloudStorageClient::CloudStorageClient(CloudStorageConfig config, std::string slotKey):
    m_config(std::move(config)),
    m_slotKey(std::move(slotKey))
{}

std::string CloudStorageClient::documentUrl() const
{
    return m_config.apiUrl + "/api/v1/documents/" + m_slotKey;
}

curl_slist* CloudStorageClient::buildHeaders() const
{
    const auto authHeader = std::string("x-api-key: ") + m_config.apiKey;
    curl_slist* headers = curl_slist_append(nullptr, authHeader.c_str());
    return curl_slist_append(headers, "Content-Type: application/json");
}

CloudStorageClient::RawResponse CloudStorageClient::performGet() const
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
        throw CloudStorageException("Cannot initialize CURL!");

    std::string response;
    curl_slist* headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, documentUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    const auto result = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
        throw CloudStorageException(std::string("API read failed: ") + curl_easy_strerror(result));

    return { httpStatus, response };
}

CloudStorageClient::RawResponse CloudStorageClient::performPut(const std::string& body) const
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
        throw CloudStorageException("Cannot initialize CURL!");

    std::string response;
    curl_slist* headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, documentUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    const auto result = curl_easy_perform(curl);

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
        throw CloudStorageException(std::string("API write failed: ") + curl_easy_strerror(result));

    return { httpStatus, response };
}

CloudDocument CloudStorageClient::parseDocumentResponse(const std::string& body)
{
    const auto json = nlohmann::json::parse(body);
    const auto contentString = json.at("content").get<std::string>();
    const auto updatedAtString = json.at("updatedAt").get<std::string>();

    nlohmann::json content = nlohmann::json::object();
    if (!contentString.empty())
        content = nlohmann::json::parse(contentString);

    return CloudDocument{ content, parseIso8601(updatedAtString) };
}

std::optional<CloudDocument> CloudStorageClient::loadDocument() const
{
    if (!m_config.isConfigured())
        return std::nullopt;

    const auto [httpStatus, body] = performGet();

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

CloudWriteResult CloudStorageClient::saveDocument(const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
{
    if (!m_config.isConfigured())
    {
        g_debug("saveDocument: NOT configured! apiUrl='%s' apiKey.empty=%d", m_config.apiUrl.c_str(), m_config.apiKey.empty());
        throw CloudStorageException("Cloud sync is not configured");
    }

    const nlohmann::json payload = {
        { "content", data.dump() },
        { "clientLastKnownUpdate", formatIso8601(clientLastKnownUpdate) }
    };

    const auto [httpStatus, body] = performPut(payload.dump());

    if (httpStatus != 200 && httpStatus != 409)
        throw CloudStorageException(std::format("API write failed with status {}", httpStatus));

    try
    {
        return CloudWriteResult{
            parseDocumentResponse(body),
            httpStatus == 409
        };
    }
    catch (const nlohmann::json::exception& e)
    {
        throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
    }
}

std::string CloudStorageClient::formatIso8601(const std::chrono::system_clock::time_point tp)
{
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::microseconds>(tp));
}

std::chrono::system_clock::time_point CloudStorageClient::parseIso8601(const std::string& str)
{
    if (str.size() < 20 || str.back() != 'Z')
        throw CloudStorageException("Invalid ISO-8601 timestamp: " + str);

    std::tm tm{};
    tm.tm_year = std::stoi(str.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(str.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(str.substr(8, 2));
    tm.tm_hour = std::stoi(str.substr(11, 2));
    tm.tm_min  = std::stoi(str.substr(14, 2));
    tm.tm_sec  = std::stoi(str.substr(17, 2));

    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));

    if (str.size() > 20 && str[19] == '.')
    {
        std::string fraction = str.substr(20, str.size() - 21); // bez końcowego 'Z'
        fraction.resize(6, '0');                                 // dopełnij/przytnij do mikrosekund
        tp += std::chrono::microseconds(std::stoll(fraction.substr(0, 6)));
    }

    return tp;
}

size_t CloudStorageClient::writeCallback(const char* ptr, const size_t size, const size_t nmemb, std::string* response)
{
    const size_t totalSize = size * nmemb;
    response->append(ptr, totalSize);
    return totalSize;
}