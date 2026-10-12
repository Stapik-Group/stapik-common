#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/stringlist.h>
#include <sigc++/signal.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace stapik::ui
{
    class ChoiceDropDown final : public Gtk::DropDown
    {
    public:
        ChoiceDropDown();

        void setLabels(const std::vector<std::string>& labels);
        void select(std::optional<std::size_t> index);
        [[nodiscard]] std::optional<std::size_t> selectedIndex() const;
        sigc::signal<void(std::size_t)>& signalChosen();

    private:
        Glib::RefPtr<Gtk::StringList> m_model;
        std::vector<std::string> m_labels;
        sigc::signal<void(std::size_t)> m_signalChosen;
        bool m_programmaticChange = false;
    };
}
