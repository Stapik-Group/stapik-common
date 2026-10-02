#pragma once

#include "StapikDialog.hpp"

#include <glibmm/ustring.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <functional>
#include <string>

struct TextInputDialogOptions
{
    Glib::ustring title;
    Glib::ustring label;
    Glib::ustring initialText;
    Glib::ustring placeholder;
    Glib::ustring confirmLabel;
    bool allowEmpty = false;
};

class TextInputDialog : public StapikDialog
{
public:
    TextInputDialog(Gtk::Window& parent, const TextInputDialogOptions& options, std::function<void(const std::string&)> onAccept);

private:
    Gtk::Label m_label;
    Gtk::Entry m_entry;
    bool m_allowEmpty;

    [[nodiscard]] std::string enteredText() const;
    void updateConfirmSensitivity();
};

void showTextInputDialog(Gtk::Window& parent, const TextInputDialogOptions& options, std::function<void(const std::string&)> onAccept);
