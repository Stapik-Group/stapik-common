#pragma once

#include "AppInfo.hpp"
#include "CommandLine.hpp"
#include "SingleInstanceGuard.hpp"

#include "stapik/storage/DirectoryLock.hpp"
#include "stapik/ui/style/AppStyleProvider.hpp"
#include "stapik/update/UpdateChecker.hpp"

#include <giomm/application.h>
#include <glibmm/refptr.h>
#include <glibmm/variantdict.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <sigc++/connection.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace stapik::app
{
    struct StapikApplicationOptions
    {
        bool singleInstance = true;
        bool rememberWindowState = true;
        bool checkForUpdates = false;
        std::filesystem::path resourcesDir;
    };

    class StapikApplication : public Gtk::Application
    {
    public:
        [[nodiscard]] static Glib::RefPtr<StapikApplication> create(const AppInfo& appInfo, StapikApplicationOptions options = {});

        [[nodiscard]] const AppInfo& appInfo() const;
        [[nodiscard]] const CommandLineOptions& commandLine() const;
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

        [[nodiscard]] bool acquireDataDirLock();
        void applyTheme();
        void startUpdateCheck();
        void announceUpdate(const update::ReleaseInfo& release);
        void onSecondInstance(const std::vector<std::filesystem::path>& files);
        [[nodiscard]] Gtk::Window* primaryWindow();
        void onWindowAdded(Gtk::Window& window);
        void installQuitAction(Gtk::Window& window);
        static void restoreWindowState(Gtk::Window& window, const std::string& key) ;
        static void saveWindowState(Gtk::Window& window, const std::string& key);

        AppInfo m_info;
        StapikApplicationOptions m_options;
        CommandLineOptions m_commandLine;
        AppStyleProvider m_styleProvider;
        std::unique_ptr<SingleInstanceGuard> m_instanceGuard;
        std::unique_ptr<storage::DirectoryLock> m_dataDirLock;
        sigc::connection m_windowAddedConnection;
        sigc::connection m_themeConnection;
        std::unique_ptr<update::UpdateChecker> m_updateChecker;
        sigc::connection m_updateConnection;
    };
}
