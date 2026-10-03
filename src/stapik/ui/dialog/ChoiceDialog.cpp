#include "ChoiceDialog.hpp"

#include "DialogUtils.hpp"

#include <algorithm>
#include <utility>

ChoiceDialog::ChoiceDialog(Gtk::Window &parent, const ChoiceDialogOptions &options,
                           std::function<void(std::size_t)> onChosen) : StapikDialog(parent, options.title)
{
    signal_response().connect([this, onChosen = std::move(onChosen)](const int response)
    {
        if (response == static_cast<int>(Gtk::ResponseType::OK) && onChosen && !m_buttons.empty())
            onChosen(selectedIndex());
    });

    if (!options.message.empty())
    {
        m_messageLabel.set_text(options.message);
        m_messageLabel.set_halign(Gtk::Align::START);
        m_messageLabel.set_wrap(true);
        contentBox().append(m_messageLabel);
    }

    for (const auto &choice: options.choices)
    {
        auto button = std::make_unique<Gtk::CheckButton>(choice);

        if (!m_buttons.empty())
            button->set_group(*m_buttons.front());

        contentBox().append(*button);
        m_buttons.push_back(std::move(button));
    }

    if (!m_buttons.empty())
        m_buttons[std::min(options.selectedIndex, m_buttons.size() - 1)]->set_active(true);

    addCancelButton();
    addOkButton(options.confirmLabel);
    set_response_sensitive(Gtk::ResponseType::OK, !m_buttons.empty());
}

std::size_t ChoiceDialog::selectedIndex() const
{
    for (std::size_t index = 0; index < m_buttons.size(); ++index)
    {
        if (m_buttons[index]->get_active())
            return index;
    }

    return 0;
}

void showChoiceDialog(Gtk::Window &parent, const ChoiceDialogOptions &options, std::function<void(std::size_t)> onChosen)
{
    showAutoDeletingDialog<ChoiceDialog>(parent, options, std::move(onChosen));
}
