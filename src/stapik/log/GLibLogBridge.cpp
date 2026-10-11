#include "GLibLogBridge.hpp"

#include "Log.hpp"

#include <glib.h>

#include <string>

namespace
{
    stapik::log::Level toLevel(const GLogLevelFlags flags)
    {
        using stapik::log::Level;

        if ((flags & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL)) != 0)
            return Level::Error;

        if ((flags & G_LOG_LEVEL_WARNING) != 0)
            return Level::Warning;

        if ((flags & G_LOG_LEVEL_DEBUG) != 0)
            return Level::Debug;

        return Level::Info;
    }

    void forward(const gchar* domain, const GLogLevelFlags flags, const gchar* message, gpointer)
    {
        const std::string text = message != nullptr ? message : "";
        const std::string prefix = domain != nullptr ? std::string(domain) + ": " : "";

        stapik::log::detail::write(toLevel(flags), prefix + text);
    }
}

namespace stapik::log
{
    void forwardGLibMessagesToLog()
    {
        g_log_set_default_handler(&forward, nullptr);
    }
}
