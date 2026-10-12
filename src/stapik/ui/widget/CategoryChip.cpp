#include "CategoryChip.hpp"

CategoryChip::CategoryChip(const stapik::domain::Category& category) :
    m_category(category)
{
    set_halign(Gtk::Align::START);
    set_ellipsize(Pango::EllipsizeMode::END);
    add_css_class(m_category.cssClass());
    set_text(m_category.name);
}

void CategoryChip::setCategory(const stapik::domain::Category& category)
{
    remove_css_class(m_category.cssClass());
    m_category = category;
    add_css_class(m_category.cssClass());
    set_text(m_category.name);
}

const stapik::domain::Category& CategoryChip::category() const
{
    return m_category;
}
