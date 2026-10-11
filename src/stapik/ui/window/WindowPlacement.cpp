#include "WindowPlacement.hpp"

#include <glibmm/main.h>
#include <gtk/gtk.h>
#include <sigc++/sigc++.h>

#include <memory>

// The platform headers come last: Xlib defines macros such as None and Bool that break other headers.
#if defined(GDK_WINDOWING_WIN32)
#include <gdk/win32/gdkwin32.h>
#include <windows.h>
#elif defined(GDK_WINDOWING_X11) && defined(STAPIK_HAS_X11)
#include <gdk/x11/gdkx.h>
#endif

namespace
{
    // Keeps the check away from the very corner, where a window cannot be grabbed by its title bar.
    constexpr int VISIBLE_MARGIN = 50;

    using stapik::ui::WindowPosition;

    GdkSurface* surfaceOf(Gtk::Window& window)
    {
        return gtk_native_get_surface(GTK_NATIVE(window.gobj()));
    }

#if defined(GDK_WINDOWING_WIN32)

    HWND handleOf(GdkSurface* surface)
    {
        return GDK_IS_WIN32_SURFACE(surface) ? gdk_win32_surface_get_handle(surface) : nullptr;
    }

    std::optional<WindowPosition> readPosition(GdkSurface* surface)
    {
        const HWND handle = handleOf(surface);
        RECT frame{};
        if (handle == nullptr || IsIconic(handle) || !GetWindowRect(handle, &frame))
            return std::nullopt;

        return WindowPosition{ .x = static_cast<int>(frame.left), .y = static_cast<int>(frame.top) };
    }

    bool isOnScreen(GdkSurface* surface, const WindowPosition& position)
    {
        if (handleOf(surface) == nullptr)
            return false;

        const POINT anchor{ position.x + VISIBLE_MARGIN, position.y + VISIBLE_MARGIN };
        return MonitorFromPoint(anchor, MONITOR_DEFAULTTONULL) != nullptr;
    }

    bool writePosition(GdkSurface* surface, const WindowPosition& position)
    {
        const HWND handle = handleOf(surface);
        if (handle == nullptr)
            return false;

        constexpr UINT flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
        return SetWindowPos(handle, nullptr, position.x, position.y, 0, 0, flags) != FALSE;
    }

#elif defined(GDK_WINDOWING_X11) && defined(STAPIK_HAS_X11)

    // Position of the client area in root window coordinates, which is also what _NET_MOVERESIZE_WINDOW
    // takes together with the static gravity, so reading and writing use the same reference point.
    constexpr long MOVE_RESIZE_SOURCE_APPLICATION = 1L << 12;
    constexpr long MOVE_RESIZE_X_VALID = 1L << 8;
    constexpr long MOVE_RESIZE_Y_VALID = 1L << 9;

    Display* displayOf(GdkSurface* surface)
    {
        return GDK_IS_X11_SURFACE(surface) ? gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface)) : nullptr;
    }

    std::optional<WindowPosition> readPosition(GdkSurface* surface)
    {
        Display* display = displayOf(surface);
        if (display == nullptr)
            return std::nullopt;

        int x = 0;
        int y = 0;
        ::Window child = None;
        if (!XTranslateCoordinates(display, gdk_x11_surface_get_xid(surface), DefaultRootWindow(display), 0, 0, &x, &y, &child))
            return std::nullopt;

        return WindowPosition{ .x = x, .y = y };
    }

    bool isOnScreen(GdkSurface* surface, const WindowPosition& position)
    {
        Display* display = displayOf(surface);
        if (display == nullptr)
            return false;

        const int anchorX = position.x + VISIBLE_MARGIN;
        const int anchorY = position.y + VISIBLE_MARGIN;
        const int screen = DefaultScreen(display);
        return anchorX >= 0 && anchorY >= 0 && anchorX < DisplayWidth(display, screen) && anchorY < DisplayHeight(display, screen);
    }

    bool writePosition(GdkSurface* surface, const WindowPosition& position)
    {
        Display* display = displayOf(surface);
        if (display == nullptr)
            return false;

        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = gdk_x11_surface_get_xid(surface);
        event.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = StaticGravity | MOVE_RESIZE_X_VALID | MOVE_RESIZE_Y_VALID | MOVE_RESIZE_SOURCE_APPLICATION;
        event.xclient.data.l[1] = position.x;
        event.xclient.data.l[2] = position.y;

        constexpr long rootMask = SubstructureRedirectMask | SubstructureNotifyMask;
        const bool sent = XSendEvent(display, DefaultRootWindow(display), False, rootMask, &event) != 0;
        XFlush(display);
        return sent;
    }

#else

    std::optional<WindowPosition> readPosition(GdkSurface*)
    {
        return std::nullopt;
    }

    bool isOnScreen(GdkSurface*, const WindowPosition&)
    {
        return false;
    }

    bool writePosition(GdkSurface*, const WindowPosition&)
    {
        return false;
    }

#endif
}

namespace stapik::ui
{
    std::optional<WindowPosition> currentWindowPosition(Gtk::Window& window)
    {
        auto* surface = surfaceOf(window);
        return surface == nullptr ? std::nullopt : readPosition(surface);
    }

    bool moveWindow(Gtk::Window& window, const WindowPosition& position)
    {
        auto* surface = surfaceOf(window);
        return surface != nullptr && isOnScreen(surface, position) && writePosition(surface, position);
    }

    void restoreWindowPositionWhenMapped(Gtk::Window& window, const WindowPosition& position)
    {
        auto connection = std::make_shared<sigc::connection>();
        *connection = window.signal_map().connect([&window, position, connection]
        {
            // The window manager reads the move request only after it has taken over the new window,
            // which happens after the map request has been sent, so the move waits for the main loop to idle.
            Glib::signal_idle().connect_once(sigc::track_object([&window, position] { moveWindow(window, position); }, window));
            connection->disconnect();
        });
    }
}
