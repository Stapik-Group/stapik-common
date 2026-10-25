#pragma once

#include "RateLimit.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

class CloudStorageException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class CloudDecryptionException : public CloudStorageException
{
public:
    using CloudStorageException::CloudStorageException;
};

class CloudAuthRejectedException : public CloudStorageException
{
public:
    using CloudStorageException::CloudStorageException;
};

class CloudDocumentTooLargeException : public CloudStorageException
{
public:
    using CloudStorageException::CloudStorageException;
};

// The cloud refused a request because the API key sent too many of them (HTTP 429) and the wait it asked for was
// too long, or too many attempts were used up, to be waited out in place.
class CloudRateLimitedException : public CloudStorageException
{
public:
    explicit CloudRateLimitedException(const std::optional<std::chrono::seconds> retryAfter, const std::optional<stapik::cloud::RateLimitState> rateLimit = std::nullopt) :
        CloudStorageException(messageFor(retryAfter)),
        m_retryAfter(retryAfter),
        m_rateLimit(rateLimit)
    {
    }

    // How long the server asked to wait before the next request, when it said so.
    [[nodiscard]] std::optional<std::chrono::seconds> retryAfter() const { return m_retryAfter; }

    // The allowance of the key as the refusing answer reported it, when it did.
    [[nodiscard]] std::optional<stapik::cloud::RateLimitState> rateLimit() const { return m_rateLimit; }

private:
    static std::string messageFor(const std::optional<std::chrono::seconds> retryAfter)
    {
        auto message = std::string("API rate limit exceeded");
        if (retryAfter)
            message += ", retry in " + std::to_string(retryAfter->count()) + " s";

        return message;
    }

    std::optional<std::chrono::seconds> m_retryAfter;
    std::optional<stapik::cloud::RateLimitState> m_rateLimit;
};
