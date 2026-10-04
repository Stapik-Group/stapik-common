#pragma once

#include "stapik/cloud/CloudStorageException.hpp"
#include "stapik/cloud/ICloudStorage.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace stapik::test
{
    class FakeCloudStorage final : public ICloudStorage
    {
    public:
        using TimePoint = std::chrono::system_clock::time_point;

        std::optional<CloudDocument> stored;
        std::map<std::string, CloudDocument> partitions;
        std::function<void()> beforeSave;

        std::atomic<bool> unreachable{ false };

        std::atomic<bool> failSaves{ false };

        mutable std::atomic<int> loadCalls{ 0 };
        mutable std::atomic<int> saveCalls{ 0 };
        mutable std::atomic<int> partitionLoadCalls{ 0 };
        mutable std::atomic<int> partitionSaveCalls{ 0 };
        mutable std::vector<std::string> loadedPartitions;
        mutable std::vector<std::string> savedPartitions;
        mutable std::vector<TimePoint> receivedBaselines;

        TimePoint nextServerTime{};
        std::chrono::seconds serverTimeStep{ 0 };

        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override
        {
            ++loadCalls;
            if (unreachable)
                throw CloudStorageException("network unreachable");

            return stored;
        }

        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, const TimePoint clientLastKnownUpdate) const override
        {
            ++saveCalls;
            receivedBaselines.push_back(clientLastKnownUpdate);

            if (unreachable || failSaves)
                throw CloudStorageException("network unreachable");

            auto& self = const_cast<FakeCloudStorage&>(*this);
            if (self.beforeSave)
                self.beforeSave();

            if (self.stored && self.stored->updatedAt > clientLastKnownUpdate)
                return CloudWriteResult{ .document = *self.stored, .conflict = true };

            self.stored = CloudDocument{ .content = data, .updatedAt = self.nextServerTime };
            self.nextServerTime += self.serverTimeStep;
            return CloudWriteResult{ .document = *self.stored, .conflict = false };
        }

        [[nodiscard]] std::vector<CloudPartitionInfo> listPartitions() const override
        {
            if (unreachable)
                throw CloudStorageException("network unreachable");

            std::vector<CloudPartitionInfo> result;
            for (const auto& [key, document] : partitions)
            {
                const auto body = document.content.dump();
                result.push_back(CloudPartitionInfo{
                    .partition = key,
                    .sizeBytes = static_cast<std::int64_t>(body.size()),
                    .contentHash = std::to_string(std::hash<std::string>{}(body)),
                    .updatedAt = document.updatedAt });
            }
            return result;
        }

        [[nodiscard]] std::optional<CloudDocument> loadPartition(const std::string& partition) const override
        {
            ++partitionLoadCalls;
            loadedPartitions.push_back(partition);
            if (unreachable)
                throw CloudStorageException("network unreachable");

            const auto found = partitions.find(partition);
            return found == partitions.end() ? std::nullopt : std::optional<CloudDocument>{ found->second };
        }

        [[nodiscard]] CloudWriteResult savePartition(const std::string& partition, const nlohmann::json& data, const TimePoint clientLastKnownUpdate) const override
        {
            ++partitionSaveCalls;
            savedPartitions.push_back(partition);
            if (unreachable || failSaves)
                throw CloudStorageException("network unreachable");

            auto& self = const_cast<FakeCloudStorage&>(*this);
            if (const auto found = self.partitions.find(partition); found != self.partitions.end() && found->second.updatedAt > clientLastKnownUpdate)
                return CloudWriteResult{ .document = found->second, .conflict = true };

            auto& slot = self.partitions[partition];
            slot = CloudDocument{ .content = data, .updatedAt = self.nextServerTime };
            self.nextServerTime += self.serverTimeStep;
            return CloudWriteResult{ .document = slot, .conflict = false };
        }

        bool deletePartition(const std::string& partition) const override
        {
            if (unreachable)
                throw CloudStorageException("network unreachable");

            return const_cast<FakeCloudStorage&>(*this).partitions.erase(partition) > 0;
        }
    };
}
