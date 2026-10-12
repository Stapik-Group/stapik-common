#pragma once

#include <glibmm/ustring.h>
#include <gtkmm/window.h>

#include <string>
#include <utility>

template<typename DialogT, typename... Args>
DialogT* showAutoDeletingDialog(Args&&... args)
{
    auto* dialog = new DialogT(std::forward<Args>(args)...);
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    dialog->signal_hide().connect([dialog] { delete dialog; });
    dialog->show();
    return dialog;
}

void openInBrowser(const std::string& url);
void showMessageDialog(Gtk::Window& parent, const Glib::ustring& primaryText, const Glib::ustring& secondaryText);
