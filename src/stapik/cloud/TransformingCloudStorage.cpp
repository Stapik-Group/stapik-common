#include "TransformingCloudStorage.hpp"

#include <utility>

namespace stapik::cloud
{
    namespace
    {
        const std::string DOCUMENT_CONTEXT = "document";
    }

    TransformingCloudStorage::TransformingCloudStorage(std::unique_ptr<ICloudStorage> inner) :
        m_inner(std::move(inner))
    {}

    std::string TransformingCloudStorage::partitionContext(const std::string& partition)
    {
        return "partition/" + partition;
    }

    std::optional<CloudDocument> TransformingCloudStorage::openIfPresent(std::optional<CloudDocument> document, const std::string& context) const
    {
        if (!document)
            return std::nullopt;

        return open(std::move(*document), context);
    }

    CloudWriteResult TransformingCloudStorage::openWriteResult(CloudWriteResult result, const nlohmann::json& sentData, const std::string& context) const
    {
        if (result.conflict)
            return { .document = open(std::move(result.document), context), .conflict = true };

        result.document.content = sentData;
        return result;
    }

    std::optional<CloudDocument> TransformingCloudStorage::loadDocument() const
    {
        return openIfPresent(m_inner->loadDocument(), DOCUMENT_CONTEXT);
    }

    CloudWriteResult TransformingCloudStorage::saveDocument(const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
    {
        return openWriteResult(m_inner->saveDocument(seal(data, DOCUMENT_CONTEXT), clientLastKnownUpdate), data, DOCUMENT_CONTEXT);
    }

    std::vector<CloudPartitionInfo> TransformingCloudStorage::listPartitions() const
    {
        return m_inner->listPartitions();
    }

    std::optional<CloudDocument> TransformingCloudStorage::loadPartition(const std::string& partition) const
    {
        return openIfPresent(m_inner->loadPartition(partition), partitionContext(partition));
    }

    CloudWriteResult TransformingCloudStorage::savePartition(const std::string& partition, const nlohmann::json& data, const std::chrono::system_clock::time_point clientLastKnownUpdate) const
    {
        const auto context = partitionContext(partition);
        return openWriteResult(m_inner->savePartition(partition, seal(data, context), clientLastKnownUpdate), data, context);
    }

    bool TransformingCloudStorage::deletePartition(const std::string& partition) const
    {
        return m_inner->deletePartition(partition);
    }
}
