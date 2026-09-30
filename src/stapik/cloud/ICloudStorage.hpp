#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <optional>

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

// Abstraction of a remote document store (a single slot). It allows the sync logic
// to be tested against a fake (FakeCloudStorage) without any network access.
//
// Contract:
//  - loadDocument() returns std::nullopt when the document does not exist or the storage
//    is not configured; network and protocol failures are reported as CloudStorageException.
//  - saveDocument() returns a CloudWriteResult; conflict == true means the server rejected
//    the write (409) and returned its own, newer version in `document`. Failures are
//    reported as CloudStorageException.
class ICloudStorage
{
public:
    virtual ~ICloudStorage() = default;

    [[nodiscard]] virtual std::optional<CloudDocument> loadDocument() const = 0;

    [[nodiscard]] virtual CloudWriteResult saveDocument(
        const nlohmann::json& data,
        std::chrono::system_clock::time_point clientLastKnownUpdate) const = 0;

protected:
    ICloudStorage() = default;
    ICloudStorage(const ICloudStorage&) = default;
    ICloudStorage& operator=(const ICloudStorage&) = default;
};
