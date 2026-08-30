#pragma once

#include "stapik/theme/Theme.hpp"

#include <gtkmm/cssprovider.h>
#include <filesystem>

class AppStyleProvider
{
public:
    explicit AppStyleProvider(std::filesystem::path resourcesDir);
    void apply(Theme theme);
private:
    std::filesystem::path m_resourcesDir;
    Glib::RefPtr<Gtk::CssProvider> m_currentProvider;
    [[nodiscard]] std::filesystem::path cssPath(Theme theme) const;
};
