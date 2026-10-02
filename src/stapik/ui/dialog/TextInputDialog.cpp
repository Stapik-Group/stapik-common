#include "TextInputDialog.hpp"

#include "DialogUtils.hpp"

#include "stapik/text/Trim.hpp"

#include <utility>

TextInputDialog::TextInputDialog(Gtk::Window& parent, const TextInputDialogOptions& options, std::function<void(const std::string&)> onAccept) :
    StapikDialog(parent, options.title),
    m_allowEmpty(options.allowEmpty)
{
    signal_response().connect([this, onAccept = std::move(onAccept)](const int response)
    {
        if (response == static_cast<int>(Gtk::ResponseType::OK) && onAccept)
            onAccept(enteredText());
    });

    m_label.set_text(options.label);
    m_label.set_halign(Gtk::Align::START);

    m_entry.set_placeholder_text(options.placeholder);
    m_entry.set_activates_default(true);
    m_entry.signal_changed().connect([this] { updateConfirmSensitivity(); });

    contentBox().append(m_label);
    contentBox().append(m_entry);

    addCancelButton();
    addOkButton(options.confirmLabel);

    m_entry.set_text(options.initialText);
    updateConfirmSensitivity();
}

std::string TextInputDialog::enteredText() const
{
    return stapik::text::trim(m_entry.get_text().raw());
}

void TextInputDialog::updateConfirmSensitivity()
{
    set_response_sensitive(Gtk::ResponseType::OK, m_allowEmpty || !enteredText().empty());
}

void showTextInputDialog(Gtk::Window& parent, const TextInputDialogOptions& options, std::function<void(const std::string&)> onAccept)
{
    showAutoDeletingDialog<TextInputDialog>(parent, options, std::move(onAccept));
}
