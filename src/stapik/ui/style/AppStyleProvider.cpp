#include "AppStyleProvider.hpp"
#include <gtkmm/stylecontext.h>

AppStyleProvider::AppStyleProvider(std::filesystem::path resourcesDir) :
    m_resourcesDir(std::move(resourcesDir)) {}

void AppStyleProvider::apply(const Theme theme)
{
    if (m_currentProvider)
        Gtk::StyleContext::remove_provider_for_display(Gdk::Display::get_default(), m_currentProvider);

    m_currentProvider = Gtk::CssProvider::create();
    m_currentProvider->signal_parsing_error().connect(
        [](const Glib::RefPtr<const Gtk::CssSection>&, const Glib::Error& error)
        {
            g_warning("CSS parsing error: %s", error.what());
        });
    m_currentProvider->load_from_path(cssPath(theme).string());
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        m_currentProvider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

std::filesystem::path AppStyleProvider::cssPath(const Theme theme) const
{
    return m_resourcesDir / (theme == Theme::Modern ? "style-modern.css" : "style-classic.css");
}