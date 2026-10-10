#pragma once

#include "AppInfo.hpp"
#include "SingleInstanceGuard.hpp"

#include "stapik/ui/style/AppStyleProvider.hpp"

#include <giomm/application.h>
#include <glibmm/refptr.h>
#include <glibmm/variantdict.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <sigc++/connection.h>

#include <filesystem>
#include <memory>
#include <string>

namespace stapik::app
{
    struct StapikApplicationOptions
    {
        bool singleInstance = true;
        bool rememberWindowState = true;
        std::filesystem::path resourcesDir;
    };

    class StapikApplication : public Gtk::Application
    {
    public:
        [[nodiscard]] static Glib::RefPtr<StapikApplication> create(const AppInfo& appInfo, StapikApplicationOptions options = {});

        [[nodiscard]] const AppInfo& appInfo() const;
        [[nodiscard]] AppStyleProvider& styleProvider();
        [[nodiscard]] const theme::ThemeRegistry& themes() const;

        void requestQuit();
    protected:
        explicit StapikApplication(const AppInfo& appInfo, StapikApplicationOptions options = {});
        ~StapikApplication() override;

        void on_startup() override;
        int on_handle_local_options(const Glib::RefPtr<Glib::VariantDict>& options) override;

        [[nodiscard]] virtual std::string windowStateKey(const Gtk::Window& window);
    private:
        static const std::string& initializeContext(const AppInfo& appInfo);

        void applyTheme();
        void onWindowAdded(Gtk::Window& window);
        void installQuitAction(Gtk::Window& window);
        static void restoreWindowState(Gtk::Window& window, const std::string& key) ;
        static void saveWindowState(const Gtk::Window& window, const std::string& key) ;

        AppInfo m_info;
        StapikApplicationOptions m_options;
        AppStyleProvider m_styleProvider;
        std::unique_ptr<SingleInstanceGuard> m_instanceGuard;
        sigc::connection m_windowAddedConnection;
        sigc::connection m_themeConnection;
    };
}
