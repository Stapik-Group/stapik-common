#include "MemoryLog.hpp"

#include "Log.hpp"

#include <mutex>

namespace
{
    std::mutex& memoryLogMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::shared_ptr<stapik::log::RingBufferSink>& memoryLogSlot()
    {
        static std::shared_ptr<stapik::log::RingBufferSink> slot;
        return slot;
    }
}

namespace stapik::log
{
    std::shared_ptr<RingBufferSink> installMemoryLog(const std::size_t capacity)
    {
        const std::lock_guard lock(memoryLogMutex());
        auto& slot = memoryLogSlot();

        if (!slot)
        {
            slot = std::make_shared<RingBufferSink>(capacity);
            addSink(slot);
        }

        return slot;
    }

    std::shared_ptr<RingBufferSink> memoryLog()
    {
        const std::lock_guard lock(memoryLogMutex());
        return memoryLogSlot();
    }
}
