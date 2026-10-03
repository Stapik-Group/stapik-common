#pragma once

#include "StapikDialog.hpp"

#include <glibmm/ustring.h>
#include <gtkmm/label.h>

#include <functional>

struct ConfirmDialogOptions
{
    Glib::ustring title;
    Glib::ustring message;
    Glib::ustring confirmLabel;
    bool destructive = false;
};

class ConfirmDialog : public StapikDialog
{
public:
    ConfirmDialog(Window& parent, const ConfirmDialogOptions& options, std::function<void()> onConfirm);

private:
    Gtk::Label m_messageLabel;
};

void showConfirmDialog(Gtk::Window& parent, const ConfirmDialogOptions& options, std::function<void()> onConfirm);
