#pragma once

#include <glibmm/main.h>
#include <glibmm/ustring.h>
#include <gtkmm/window.h>

#include <memory>
#include <string>
#include <utility>

// Deletes a window that was created with new, once it is hidden. The deletion waits for the main loop to idle:
// the window is hidden from inside its own signals (a response, a close request), and deleting it right there
// would free the widget while GTK is still emitting a signal on it.
template<typename WindowT>
void deleteOnHide(WindowT* window)
{
    const auto scheduled = std::make_shared<bool>(false);
    window->signal_hide().connect([window, scheduled]
    {
        if (*scheduled)
            return;

        *scheduled = true;
        Glib::signal_idle().connect_once([window] { delete window; });
    });
}

template<typename DialogT, typename... Args>
DialogT* showAutoDeletingDialog(Args&&... args)
{
    auto* dialog = new DialogT(std::forward<Args>(args)...);
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    deleteOnHide(dialog);
    dialog->show();
    return dialog;
}

void openInBrowser(const std::string& url);
void showMessageDialog(Gtk::Window& parent, const Glib::ustring& primaryText, const Glib::ustring& secondaryText);
