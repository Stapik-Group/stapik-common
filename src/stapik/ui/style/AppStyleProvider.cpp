#include "AppStyleProvider.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/theme/FontScale.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"

#include <gtk/gtk.h>
#include <gtkmm/stylecontext.h>

#include <fstream>
#include <optional>
#include <sstream>

namespace
{
    constexpr auto FALLBACK_THEME_ID = "classic";

    std::optional<std::string> readText(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            return std::nullopt;

        std::ostringstream content;
        content << stream.rdbuf();
        return content.str();
    }

    void loadCss(const Glib::RefPtr<Gtk::CssProvider>& provider, const std::string& css)
    {
#if GTK_CHECK_VERSION(4, 12, 0)
        gtk_css_provider_load_from_string(provider->gobj(), css.c_str());
#else
        gtk_css_provider_load_from_data(provider->gobj(), css.c_str(), -1);
#endif
    }
}

AppStyleProvider::AppStyleProvider(const std::filesystem::path& resourcesDir)
{
    m_registry.addDirectory(resourcesDir);
}

AppStyleProvider::AppStyleProvider(const std::vector<std::filesystem::path>& resourcesDirs)
{
    for (const auto& resourcesDir : resourcesDirs)
        m_registry.addDirectory(resourcesDir);
}

AppStyleProvider AppStyleProvider::withCommonThemes(std::filesystem::path appResourcesDir)
{
    std::vector<std::filesystem::path> resourcesDirs;

    const auto commonResourcesDir = AppPaths::commonResourcesDir();
    if (std::error_code errorCode; std::filesystem::is_directory(commonResourcesDir, errorCode))
        resourcesDirs.push_back(commonResourcesDir);
    else
        stapik::log::debug("stapik-common resources not found at {}, using the application's themes only", stapik::storage::pathText(commonResourcesDir));

    resourcesDirs.push_back(std::move(appResourcesDir));
    return AppStyleProvider(resourcesDirs);
}

void AppStyleProvider::apply(const Theme theme)
{
    apply(themeToFileString(theme));
}

void AppStyleProvider::apply(const std::string& themeId, const int fontScalePercent)
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
                stapik::log::warning("CSS parsing error in {}: {}", stapik::storage::pathText(file), error.what());
            });

        const auto css = readText(file);
        if (!css)
        {
            stapik::log::warning("Cannot read CSS file {}", stapik::storage::pathText(file));
            continue;
        }

        loadCss(provider, stapik::theme::scaleFontSizes(*css, fontScalePercent));

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
