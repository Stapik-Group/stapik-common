#include "SectionHeader.hpp"

#include "stapik/locale/LocaleManager.hpp"

#include <utility>

namespace
{
    constexpr int SPACING = 8;
}

SectionHeader::SectionHeader() :
    Box(Gtk::Orientation::HORIZONTAL, SPACING)
{
    add_css_class("stapik-section-header");

    m_title.set_halign(Gtk::Align::START);
    m_title.set_hexpand(true);
    append(m_title);

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { refresh(); });
}

SectionHeader::SectionHeader(std::string titleKey) :
    SectionHeader()
{
    setTitleKey(std::move(titleKey));
}

SectionHeader::~SectionHeader()
{
    m_localeConnection.disconnect();
}

void SectionHeader::setTitleKey(std::string titleKey)
{
    m_titleKey = std::move(titleKey);
    refresh();
}

void SectionHeader::setTitle(const std::string& title)
{
    m_titleKey.reset();
    m_title.set_text(title);
}

void SectionHeader::addAction(Widget& action)
{
    append(action);
}

void SectionHeader::refresh()
{
    if (m_titleKey)
        m_title.set_text(LocaleManager::instance().translate(*m_titleKey));
}
