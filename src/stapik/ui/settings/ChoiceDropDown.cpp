#include "ChoiceDropDown.hpp"

#include <glibmm/ustring.h>
#include <gtk/gtk.h>

namespace stapik::ui
{
    ChoiceDropDown::ChoiceDropDown() :
        m_model(Gtk::StringList::create(std::vector<Glib::ustring>{}))
    {
        set_model(m_model);
        set_halign(Gtk::Align::START);
        set_valign(Gtk::Align::CENTER);

        property_selected().signal_changed().connect([this]
        {
            if (m_programmaticChange)
                return;

            if (const auto index = selectedIndex())
                m_signalChosen.emit(*index);
        });
    }

    void ChoiceDropDown::setLabels(const std::vector<std::string>& labels)
    {
        if (labels == m_labels)
            return;

        const auto previous = selectedIndex();
        m_labels = labels;

        const std::vector<Glib::ustring> items(labels.begin(), labels.end());

        m_programmaticChange = true;
        m_model->splice(0, m_model->get_n_items(), items);
        m_programmaticChange = false;

        select(previous);
    }

    void ChoiceDropDown::select(const std::optional<std::size_t> index)
    {
        const auto position = index && *index < m_labels.size() ? static_cast<guint>(*index) : GTK_INVALID_LIST_POSITION;

        m_programmaticChange = true;
        set_selected(position);
        m_programmaticChange = false;
    }

    std::optional<std::size_t> ChoiceDropDown::selectedIndex() const
    {
        const auto position = get_selected();
        if (position == GTK_INVALID_LIST_POSITION)
            return std::nullopt;

        return position;
    }

    sigc::signal<void(std::size_t)>& ChoiceDropDown::signalChosen()
    {
        return m_signalChosen;
    }
}
