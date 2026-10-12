#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include <optional>
#include <string>

class EmptyState : public Gtk::Box
{
public:
    static constexpr auto DEFAULT_ICON = "folder-open-symbolic";
    static constexpr auto DEFAULT_TITLE_KEY = "emptyState.noData";

    explicit EmptyState(const std::string &iconName = DEFAULT_ICON, std::string titleKey = DEFAULT_TITLE_KEY);
    ~EmptyState() override;

    EmptyState(const EmptyState&) = delete;
    EmptyState& operator=(const EmptyState&) = delete;

    void setIcon(const std::string& iconName);
    void setTitleKey(std::string titleKey);
    void setDescriptionKey(std::optional<std::string> descriptionKey);
    void setActionKey(std::optional<std::string> actionKey);

    sigc::signal<void()>& signalAction();

private:
    void refresh();

    Gtk::Image m_icon;
    Gtk::Label m_title;
    Gtk::Label m_description;
    Gtk::Button m_action;
    std::string m_titleKey;
    std::optional<std::string> m_descriptionKey;
    std::optional<std::string> m_actionKey;
    sigc::signal<void()> m_signalAction;
    sigc::connection m_localeConnection;
};
