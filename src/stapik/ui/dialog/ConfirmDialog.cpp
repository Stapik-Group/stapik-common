#include "ConfirmDialog.hpp"

#include "DialogUtils.hpp"

#include <utility>

ConfirmDialog::ConfirmDialog(Gtk::Window& parent, const ConfirmDialogOptions& options, std::function<void()> onConfirm) :
    StapikDialog(parent, options.title)
{
    signal_response().connect([onConfirm = std::move(onConfirm)](const int response)
    {
        if (response == static_cast<int>(Gtk::ResponseType::OK) && onConfirm)
            onConfirm();
    });

    m_messageLabel.set_text(options.message);
    m_messageLabel.set_halign(Gtk::Align::START);
    m_messageLabel.set_wrap(true);
    contentBox().append(m_messageLabel);

    addCancelButton();
    auto* confirmButton = addOkButton(options.confirmLabel);

    if (options.destructive)
    {
        confirmButton->add_css_class("destructive-action");
        set_default_response(Gtk::ResponseType::CANCEL);
    }
}

void showConfirmDialog(Gtk::Window& parent, const ConfirmDialogOptions& options, std::function<void()> onConfirm)
{
    showAutoDeletingDialog<ConfirmDialog>(parent, options, std::move(onConfirm));
}
