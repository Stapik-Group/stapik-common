#pragma once

#include "stapik/theme/Theme.hpp"
#include "stapik/theme/ThemeRegistry.hpp"

#include <gtkmm/cssprovider.h>
#include <filesystem>
#include <string>
#include <vector>

class AppStyleProvider
{
public:
    explicit AppStyleProvider(std::filesystem::path resourcesDir);
    explicit AppStyleProvider(const std::vector<std::filesystem::path>& resourcesDirs);
    [[nodiscard]] static AppStyleProvider withCommonThemes(std::filesystem::path appResourcesDir);
    void apply(Theme theme);
    void apply(const std::string& themeId);
    [[nodiscard]] const stapik::theme::ThemeRegistry& themes() const;
private:
    void removeCurrentProviders();
    stapik::theme::ThemeRegistry m_registry;
    std::vector<Glib::RefPtr<Gtk::CssProvider>> m_currentProviders;
};
