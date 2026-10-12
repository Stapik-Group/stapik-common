#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <sigc++/connection.h>

#include <optional>
#include <string>

class SectionHeader : public Gtk::Box
{
public:
    SectionHeader();
    explicit SectionHeader(std::string titleKey);
    ~SectionHeader() override;

    SectionHeader(const SectionHeader&) = delete;
    SectionHeader& operator=(const SectionHeader&) = delete;

    void setTitleKey(std::string titleKey);
    void setTitle(const std::string& title);
    void addAction(Gtk::Widget& action);

private:
    void refresh();

    Gtk::Label m_title;
    std::optional<std::string> m_titleKey;
    sigc::connection m_localeConnection;
};
