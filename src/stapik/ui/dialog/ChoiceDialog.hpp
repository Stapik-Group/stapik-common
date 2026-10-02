#pragma once

#include "StapikDialog.hpp"

#include <glibmm/ustring.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

struct ChoiceDialogOptions
{
    Glib::ustring title;
    Glib::ustring message;
    std::vector<Glib::ustring> choices;
    std::size_t selectedIndex = 0;
    Glib::ustring confirmLabel;
};

class ChoiceDialog : public StapikDialog
{
public:
    ChoiceDialog(Gtk::Window &parent, const ChoiceDialogOptions &options, std::function<void(std::size_t)> onChosen);

private:
    Gtk::Label m_messageLabel;
    std::vector<std::unique_ptr<Gtk::CheckButton> > m_buttons;

    [[nodiscard]] std::size_t selectedIndex() const;
};

void showChoiceDialog(Gtk::Window &parent, const ChoiceDialogOptions &options, std::function<void(std::size_t)> onChosen);
