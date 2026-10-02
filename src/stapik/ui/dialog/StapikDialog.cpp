#include "StapikDialog.hpp"

#include "stapik/locale/LocaleManager.hpp"

StapikDialog::StapikDialog(Gtk::Window& parent, const Glib::ustring& title) :
    Gtk::Dialog(title, parent, true),
    m_contentBox(Gtk::Orientation::VERTICAL, CONTENT_SPACING)
{
    add_css_class("stapik-dialog");

    m_contentBox.set_margin(CONTENT_MARGIN);
    get_content_area()->append(m_contentBox);

    set_default_size(DEFAULT_WIDTH, -1);
}

Gtk::Box& StapikDialog::contentBox()
{
    return m_contentBox;
}

Gtk::Widget* StapikDialog::addCancelButton()
{
    return add_button(LocaleManager::instance().translate("dialog.button.cancel"), Gtk::ResponseType::CANCEL);
}

Gtk::Widget* StapikDialog::addOkButton(const Glib::ustring& label)
{
    const auto text = label.empty() ? Glib::ustring(LocaleManager::instance().translate("dialog.button.ok")) : label;
    auto* button = add_button(text, Gtk::ResponseType::OK);
    set_default_response(Gtk::ResponseType::OK);
    return button;
}
