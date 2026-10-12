#pragma once

#include "WindowState.hpp"

#include <gtkmm/window.h>

#include <optional>

namespace stapik::ui
{
    // GTK 4 has no window position API, so this talks to the windowing system directly. That works on X11
    // and Windows; on Wayland a client cannot know or choose its position, and these functions do nothing.
    [[nodiscard]] std::optional<WindowPosition> currentWindowPosition(Gtk::Window& window);

    // Fails when the position is outside the screen, for example after a monitor was disconnected.
    bool moveWindow(Gtk::Window& window, const WindowPosition& position);

    // A window can only be moved once it is mapped, so the move is made right after its first mapping.
    void restoreWindowPositionWhenMapped(Gtk::Window& window, const WindowPosition& position);
}
