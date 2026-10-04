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

// Abstraction of a remote document store (a single slot). It allows the sync logic
// to be tested against a fake (FakeCloudStorage) without any network access.
//
// Contract:
//  - loadDocument() returns std::nullopt when the document does not exist or the storage
//    is not configured; network and protocol failures are reported as CloudStorageException.
//  - saveDocument() returns a CloudWriteResult; conflict == true means the server rejected
//    the write (409) and returned its own, newer version in `document`. Failures are
//    reported as CloudStorageException.
//  - Partitions are independent named documents inside the same slot (e.g. one per archived
//    year) with their own content, version and conflict handling. listPartitions() does not
//    include the main document. loadPartition() returns std::nullopt when the partition does
//    not exist; savePartition() creates it on first write and follows the same conflict contract
//    as saveDocument(); deletePartition() returns false when there was nothing to delete.
//    Storages without partition support throw CloudStorageException from all four calls.
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

    virtual bool deletePartition(const std::string&) const
    {
        throw CloudStorageException(PARTITIONS_UNSUPPORTED);
    }

protected:
    static constexpr const char* PARTITIONS_UNSUPPORTED = "Cloud storage does not support document partitions";

    ICloudStorage() = default;
    ICloudStorage(const ICloudStorage&) = default;
    ICloudStorage& operator=(const ICloudStorage&) = default;
};
