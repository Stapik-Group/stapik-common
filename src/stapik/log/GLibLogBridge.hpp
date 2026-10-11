#pragma once

namespace stapik::log
{
    // Sends messages logged through GLib (g_warning, GTK and other libraries) to the log sinks as well.
    // Structured logging that does not go through g_log() is not covered.
    void forwardGLibMessagesToLog();
}
