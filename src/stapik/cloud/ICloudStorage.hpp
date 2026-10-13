#pragma once

#include "CloudStorageException.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CloudDocument
{
    nlohmann::json content;
    std::chrono::system_clock::time_point updatedAt;
    // Quoted ETag header of the response the document came from, empty when the server sent none.
    std::string etag;
};

struct CloudWriteResult
{
    CloudDocument document;
    bool conflict{};
};

struct CloudPartitionInfo
{
    std::string partition;
    std::int64_t sizeBytes{};
    std::string contentHash;
    std::chrono::system_clock::time_point updatedAt;
};

// One entry of the change feed: a document of the key's extension, main (no partition) or partition, that changed.
// A deleted document stays in the feed with deleted == true.
struct CloudChange
{
    std::string slotKey;
    std::optional<std::string> partition;
    std::int64_t sizeBytes{};
    std::string contentHash;
    std::chrono::system_clock::time_point updatedAt;
    bool deleted{};
};

struct CloudChangeList
{
    // The moment on the server's clock to pass as `since` to the next call, so that no change is missed or repeated.
    std::chrono::system_clock::time_point serverTime;
    std::vector<CloudChange> changes;
};

// Abstraction of a remote document store (a single slot). It allows the sync logic
// to be tested against a fake (FakeCloudStorage) without any network access.
//
// Contract:
//  - loadDocument() returns std::nullopt when the document does not exist or the storage
//    is not configured; network and protocol failures are reported as CloudStorageException.
//  - saveDocument() returns a CloudWriteResult; conflict == true means the write was not applied
//    because the cloud holds another version than the one the client last knew (the server
//    answered 409, or a read before the write already showed it) and its own version is in
//    `document`. Failures are reported as CloudStorageException.
//  - Partitions are independent named documents inside the same slot (e.g. one per archived
//    year) with their own content, version and conflict handling. listPartitions() does not
//    include the main document. loadPartition() returns std::nullopt when the partition does
//    not exist; savePartition() creates it on first write and follows the same conflict contract
//    as saveDocument(); deletePartition() returns false when there was nothing to delete.
//    Storages without partition support throw CloudStorageException from all four calls.
//  - listChanges() lists, oldest first, what changed in any slot of the key's extension after `since`,
//    deleted documents included (not only this storage's slot, so callers filter by slotKey). The contents
//    are not part of the feed. Storages without a change feed throw CloudStorageException.
class ICloudStorage
{
public:
    virtual ~ICloudStorage() = default;

    [[nodiscard]] virtual std::optional<CloudDocument> loadDocument() const = 0;

    [[nodiscard]] virtual CloudWriteResult saveDocument(
        const nlohmann::json& data,
        std::chrono::system_clock::time_point clientLastKnownUpdate) const = 0;

    [[nodiscard]] virtual std::vector<CloudPartitionInfo> listPartitions() const
    {
        throw CloudStorageException(PARTITIONS_UNSUPPORTED);
    }

    [[nodiscard]] virtual std::optional<CloudDocument> loadPartition(const std::string&) const
    {
        throw CloudStorageException(PARTITIONS_UNSUPPORTED);
    }

    [[nodiscard]] virtual CloudWriteResult savePartition(
        const std::string&,
        const nlohmann::json&,
        std::chrono::system_clock::time_point) const
    {
        throw CloudStorageException(PARTITIONS_UNSUPPORTED);
    }

    [[nodiscard]] virtual bool deletePartition(const std::string&) const
    {
        throw CloudStorageException(PARTITIONS_UNSUPPORTED);
    }

    [[nodiscard]] virtual CloudChangeList listChanges(std::chrono::system_clock::time_point) const
    {
        throw CloudStorageException(CHANGES_UNSUPPORTED);
    }

protected:
    static constexpr const char* PARTITIONS_UNSUPPORTED = "Cloud storage does not support document partitions";
    static constexpr const char* CHANGES_UNSUPPORTED = "Cloud storage does not support the change feed";

    ICloudStorage() = default;
    ICloudStorage(const ICloudStorage&) = default;
    ICloudStorage& operator=(const ICloudStorage&) = default;
};
