#pragma once

#include "stapik/domain/Category.hpp"

#include <gtkmm/label.h>

class CategoryChip : public Gtk::Label
{
public:
    explicit CategoryChip(const stapik::domain::Category& category);

    void setCategory(const stapik::domain::Category& category);
    [[nodiscard]] const stapik::domain::Category& category() const;

private:
    stapik::domain::Category m_category;
};
