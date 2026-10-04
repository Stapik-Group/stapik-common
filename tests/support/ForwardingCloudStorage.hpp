#pragma once

#include "FakeCloudStorage.hpp"

#include <optional>
#include <string>
#include <vector>

namespace stapik::test
{
    class ForwardingCloudStorage final : public ICloudStorage
    {
    public:
        explicit ForwardingCloudStorage(FakeCloudStorage& target) :
            m_target(target)
        {}

        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override
        {
            return m_target.loadDocument();
        }

        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, const FakeCloudStorage::TimePoint baseline) const override
        {
            return m_target.saveDocument(data, baseline);
        }

        [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override
        {
            return m_target.listPartitions();
        }

        [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override
        {
            return m_target.loadPartition(partition);
        }

        [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, const FakeCloudStorage::TimePoint baseline) const override
        {
            return m_target.savePartition(partition, data, baseline);
        }

        bool deletePartition(const std::string& partition) const override
        {
            return m_target.deletePartition(partition);
        }

    private:
        FakeCloudStorage& m_target;
    };
}
