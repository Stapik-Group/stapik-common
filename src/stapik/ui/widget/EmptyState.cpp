#include "EmptyState.hpp"

#include "stapik/locale/LocaleManager.hpp"

#include <utility>

namespace
{
    constexpr int ICON_PIXEL_SIZE = 48;
    constexpr int SPACING = 8;
}

EmptyState::EmptyState(const std::string &iconName, std::string titleKey) :
    Box(Gtk::Orientation::VERTICAL, SPACING),
    m_titleKey(std::move(titleKey))
{
    add_css_class("stapik-empty-state");
    set_halign(Gtk::Align::CENTER);
    set_valign(Gtk::Align::CENTER);
    set_hexpand(true);
    set_vexpand(true);

    m_icon.add_css_class("stapik-empty-state-icon");
    m_icon.set_pixel_size(ICON_PIXEL_SIZE);
    setIcon(iconName);

    m_title.add_css_class("stapik-empty-state-title");

    m_description.add_css_class("stapik-empty-state-description");
    m_description.set_wrap(true);
    m_description.set_justify(Gtk::Justification::CENTER);

    m_action.set_halign(Gtk::Align::CENTER);
    m_action.signal_clicked().connect([this] { m_signalAction.emit(); });

    append(m_icon);
    append(m_title);
    append(m_description);
    append(m_action);

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { refresh(); });

    refresh();
}

EmptyState::~EmptyState()
{
    m_localeConnection.disconnect();
}

void EmptyState::setIcon(const std::string& iconName)
{
    m_icon.set_from_icon_name(iconName);
    m_icon.set_visible(!iconName.empty());
}

void EmptyState::setTitleKey(std::string titleKey)
{
    m_titleKey = std::move(titleKey);
    refresh();
}

void EmptyState::setDescriptionKey(std::optional<std::string> descriptionKey)
{
    m_descriptionKey = std::move(descriptionKey);
    refresh();
}

void EmptyState::setActionKey(std::optional<std::string> actionKey)
{
    m_actionKey = std::move(actionKey);
    refresh();
}

sigc::signal<void()>& EmptyState::signalAction()
{
    return m_signalAction;
}

void EmptyState::refresh()
{
    const auto& locale = LocaleManager::instance();

    m_title.set_text(locale.translate(m_titleKey));

    m_description.set_visible(m_descriptionKey.has_value());
    if (m_descriptionKey)
        m_description.set_text(locale.translate(*m_descriptionKey));

    m_action.set_visible(m_actionKey.has_value());
    if (m_actionKey)
        m_action.set_label(locale.translate(*m_actionKey));
}
