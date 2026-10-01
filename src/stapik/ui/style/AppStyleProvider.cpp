#include "AppStyleProvider.hpp"

#include "stapik/log/Log.hpp"

#include <gtkmm/stylecontext.h>

namespace
{
    constexpr auto FALLBACK_THEME_ID = "classic";
}

AppStyleProvider::AppStyleProvider(std::filesystem::path resourcesDir)
{
    m_registry.addDirectory(resourcesDir);
}

AppStyleProvider::AppStyleProvider(const std::vector<std::filesystem::path>& resourcesDirs)
{
    for (const auto& resourcesDir : resourcesDirs)
        m_registry.addDirectory(resourcesDir);
}

void AppStyleProvider::apply(const Theme theme)
{
    apply(themeToFileString(theme));
}

void AppStyleProvider::apply(const std::string& themeId)
{
    removeCurrentProviders();

    const auto* theme = m_registry.find(themeId);
    if (theme == nullptr)
    {
        stapik::log::warning("Unknown theme '{}', using '{}'", themeId, FALLBACK_THEME_ID);
        theme = m_registry.find(FALLBACK_THEME_ID);
    }

    if (theme == nullptr)
    {
        stapik::log::warning("No theme CSS available, the default GTK style is used");
        return;
    }

    guint priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
    for (const auto& file : theme->files)
    {
        auto provider = Gtk::CssProvider::create();
        provider->signal_parsing_error().connect(
            [file](const Glib::RefPtr<const Gtk::CssSection>&, const Glib::Error& error)
            {
                stapik::log::warning("CSS parsing error in {}: {}", file.string(), error.what());
            });

        try
        {
            provider->load_from_path(file.string());
        }
        catch (const Glib::Error& error)
        {
            stapik::log::warning("Cannot load CSS file {}: {}", file.string(), error.what());
            continue;
        }

        Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), provider, priority);
        m_currentProviders.push_back(provider);
        ++priority;
    }
}

const stapik::theme::ThemeRegistry& AppStyleProvider::themes() const
{
    return m_registry;
}

void AppStyleProvider::removeCurrentProviders()
{
    for (const auto& provider : m_currentProviders)
        Gtk::StyleContext::remove_provider_for_display(Gdk::Display::get_default(), provider);

    m_currentProviders.clear();
}
