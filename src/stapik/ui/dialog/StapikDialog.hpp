#pragma once

#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/dialog.h>
#include <gtkmm/widget.h>

class StapikDialog : public Gtk::Dialog
{
public:
    StapikDialog(Window& parent, const Glib::ustring& title);

protected:
    [[nodiscard]] Gtk::Box& contentBox();
    Widget* addCancelButton();
    Widget* addOkButton(const Glib::ustring& label = {});

private:
    static constexpr int CONTENT_SPACING = 8;
    static constexpr int CONTENT_MARGIN = 16;
    static constexpr int DEFAULT_WIDTH = 400;

    Gtk::Box m_contentBox;
};
