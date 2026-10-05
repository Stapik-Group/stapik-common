#pragma once

#include "PartitionKey.hpp"
#include "SyncCoordinator.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace stapik::sync
{
    template<SyncableDocument DocumentType>
    class PartitionedSyncService
    {
    public:
        explicit PartitionedSyncService(ICloudStorage& cloudStorage) :
            m_cloudStorage(cloudStorage)
        {}

        [[nodiscard]] SyncOutcome<DocumentType> resolve(const std::string& partition, const DocumentType& localDocument)
        {
            return remember(partition, coordinatorFor(partition).resolveOnConnect(localDocument));
        }

        [[nodiscard]] std::optional<SyncOutcome<DocumentType>> resolveIfNotLoaded(const std::string& partition, const DocumentType& localDocument)
        {
            if (isLoaded(partition))
                return std::nullopt;

            return resolve(partition, localDocument);
        }

        [[nodiscard]] SyncOutcome<DocumentType> push(const std::string& partition, const DocumentType& localDocument)
        {
            return remember(partition, coordinatorFor(partition).pushLocalChange(localDocument));
        }

        [[nodiscard]] bool isLoaded(const std::string& partition) const
        {
            return m_loaded.contains(partition);
        }

        void forget(const std::string& partition)
        {
            m_loaded.erase(partition);
        }

        void reset()
        {
            m_loaded.clear();
        }

        [[nodiscard]] std::optional<std::vector<std::string>> availablePartitions() const
        {
            try
            {
                std::vector<std::string> keys;
                for (const auto& info : m_cloudStorage.listPartitions())
                    keys.push_back(info.partition);

                std::ranges::sort(keys);
                return keys;
            }
            catch (const CloudStorageException& exception)
            {
                log::info("Cannot list cloud partitions: {}", exception.what());
                return std::nullopt;
            }
        }

        [[nodiscard]] std::optional<std::vector<int>> availableYears() const
        {
            const auto keys = availablePartitions();
            if (!keys)
                return std::nullopt;

            std::vector<int> years;
            for (const auto& key : *keys)
            {
                if (const auto year = yearFromPartitionKey(key))
                    years.push_back(*year);
            }

            std::ranges::sort(years);
            return years;
        }

    private:
        [[nodiscard]] SyncCoordinator<DocumentType> coordinatorFor(const std::string& partition) const
        {
            return SyncCoordinator<DocumentType>{ m_cloudStorage, partition };
        }

        SyncOutcome<DocumentType> remember(const std::string& partition, SyncOutcome<DocumentType> outcome)
        {
            if (outcome.state == SyncState::Synchronized || outcome.replacesLocal())
                m_loaded.insert(partition);

            return outcome;
        }

        ICloudStorage& m_cloudStorage;
        std::set<std::string> m_loaded;
    };
}
