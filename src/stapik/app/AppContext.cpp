#include "AppContext.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/settings/AppSettings.hpp"

#include <cassert>
#include <memory>
#include <mutex>
#include <utility>

namespace
{
    std::mutex& contextMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::unique_ptr<stapik::app::AppContext>& contextSlot()
    {
        static std::unique_ptr<stapik::app::AppContext> slot;
        return slot;
    }
}

namespace stapik::app
{
    AppContext::AppContext(AppInfo appInfo, const bool createdFromLegacyName) :
        m_info(std::move(appInfo)),
        m_createdFromLegacyName(createdFromLegacyName)
    {}

    bool AppContext::initialize(AppInfo appInfo)
    {
        const std::lock_guard lock(contextMutex());
        auto& slot = contextSlot();

        if (appInfo.internalName.empty())
            log::warning("AppInfo::internalName is empty; settings and user data paths will not be app-specific");

        if (slot)
        {
            if (const bool canReplace = slot->m_createdFromLegacyName && slot->m_info.internalName == appInfo.internalName; !canReplace)
            {
                log::warning("AppContext is already initialized for '{}'; ignoring initialize() for '{}'", slot->m_info.internalName, appInfo.internalName);
                return false;
            }
        }

        slot.reset(new AppContext(std::move(appInfo), false));
        return true;
    }

    void AppContext::initializeFromLegacyName(const std::string& appName)
    {
        const std::lock_guard lock(contextMutex());
        auto& slot = contextSlot();

        if (slot)
        {
            if (slot->m_info.internalName != appName)
            {
                log::warning("Application name '{}' passed to instance(appName) is ignored: the context is already initialized for '{}'", appName, slot->m_info.internalName);
            }
            return;
        }

        log::warning("instance(appName) is deprecated; call stapik::app::AppContext::initialize() at the start of main() instead");

        AppInfo appInfo;
        appInfo.internalName = appName;
        slot.reset(new AppContext(std::move(appInfo), true));
    }

    bool AppContext::isInitialized()
    {
        const std::lock_guard lock(contextMutex());
        return contextSlot() != nullptr;
    }

    const AppContext& AppContext::instance()
    {
        const std::lock_guard lock(contextMutex());
        auto& slot = contextSlot();

        if (!slot)
        {
            log::error("AppContext::instance() called before AppContext::initialize(); using an empty application name");
            assert(false && "AppContext::initialize() must be called before using managers");
            slot.reset(new AppContext(AppInfo{}, false));
        }

        return *slot;
    }

    void AppContext::resetForTests()
    {
        const std::lock_guard lock(contextMutex());
        contextSlot().reset();
    }

    const AppInfo& AppContext::info() const
    {
        return m_info;
    }

    settings::SettingsStore& AppContext::settings() const
    {
        return settings::appSettingsStore(m_info.internalName);
    }
}
