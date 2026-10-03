#pragma once

#include "stapik/cloud/CloudStorageException.hpp"
#include "stapik/cloud/ICloudStorage.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <vector>

namespace stapik::test
{
    class FakeCloudStorage final : public ICloudStorage
    {
    public:
        using TimePoint = std::chrono::system_clock::time_point;

        std::optional<CloudDocument> stored;
        std::function<void()> beforeSave;

        std::atomic<bool> unreachable{ false };

        std::atomic<bool> failSaves{ false };

        mutable std::atomic<int> loadCalls{ 0 };
        mutable std::atomic<int> saveCalls{ 0 };
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
    };
}
