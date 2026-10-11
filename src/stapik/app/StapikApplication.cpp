#include "StapikApplication.hpp"

#include "AppContext.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/theme/ThemeManager.hpp"
#include "stapik/ui/dialog/UpdateDialog.hpp"
#include "stapik/ui/menu/StandardMenu.hpp"
#include "stapik/ui/window/WindowPlacement.hpp"
#include "stapik/ui/window/WindowState.hpp"

#include <giomm/actionmap.h>
#include <giomm/file.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>

#include <cstdlib>
#include <format>
#include <iostream>
#include <utility>

namespace
{
    constexpr auto MAIN_WINDOW_STATE_KEY = "window";
    constexpr auto QUIT_ACTION = "quit";

    std::filesystem::path resolveResourcesDir(const std::filesystem::path& configured)
    {
        return configured.empty() ? AppPaths::resourcesDir() : configured;
    }
}

namespace stapik::app
{
    Glib::RefPtr<StapikApplication> StapikApplication::create(const AppInfo& appInfo, StapikApplicationOptions options)
    {
        return Glib::make_refptr_for_instance(new StapikApplication(appInfo, std::move(options)));
    }

    const std::string& StapikApplication::initializeContext(const AppInfo& appInfo)
    {
        AppContext::initialize(appInfo);
        return AppContext::instance().info().applicationId;
    }

    StapikApplication::StapikApplication(const AppInfo& appInfo, StapikApplicationOptions options) :
        Application(initializeContext(appInfo)),
        m_info(AppContext::instance().info()),
        m_options(std::move(options)),
        m_styleProvider(AppStyleProvider::withCommonThemes(resolveResourcesDir(m_options.resourcesDir)))
    {
        addCommandLineOptions(*this);

        m_windowAddedConnection = signal_window_added().connect([this](Gtk::Window* window)
        {
            if (window != nullptr)
                onWindowAdded(*window);
        });
    }

    StapikApplication::~StapikApplication()
    {
        m_windowAddedConnection.disconnect();
        m_themeConnection.disconnect();
        m_updateConnection.disconnect();
    }

    const AppInfo& StapikApplication::appInfo() const
    {
        return m_info;
    }

    const CommandLineOptions& StapikApplication::commandLine() const
    {
        return m_commandLine;
    }

    AppStyleProvider& StapikApplication::styleProvider()
    {
        return m_styleProvider;
    }

    const theme::ThemeRegistry& StapikApplication::themes() const
    {
        return m_styleProvider.themes();
    }

    int StapikApplication::on_handle_local_options(const Glib::RefPtr<Glib::VariantDict>& options)
    {
        if (const int status = Application::on_handle_local_options(options); status >= 0)
            return status;

        const auto parsed = readCommandLineOptions(*options);
        if (!parsed.ok())
        {
            std::cerr << std::format("{}: {}\nTry '--help' for more information.\n", m_info.internalName, parsed.error);
            return EXIT_FAILURE;
        }

        if (parsed.options.showVersion)
        {
            std::cout << versionLine(m_info) << '\n';
            return EXIT_SUCCESS;
        }

        if (m_options.singleInstance && !m_instanceGuard)
        {
            const auto& displayName = m_info.displayName.empty() ? m_info.internalName : m_info.displayName;
            m_instanceGuard = SingleInstanceGuard::acquire(m_info.applicationId, displayName, parsed.options.files,
                [this](const std::vector<std::filesystem::path>& files) { onSecondInstance(files); });

            if (!m_instanceGuard)
                return EXIT_SUCCESS;
        }

        m_commandLine = parsed.options;
        applyCommandLineOptions(m_commandLine, m_info.internalName);

        if (m_options.singleInstance && !acquireDataDirLock())
            return EXIT_FAILURE;

        return -1;
    }

    bool StapikApplication::acquireDataDirLock()
    {
        // Registering shows whether GApplication already has a primary instance. That one owns the data, and
        // this process is only forwarded to it, so it must not take the lock.
        if (!register_application() || is_remote())
            return true;

        m_dataDirLock = storage::DirectoryLock::tryAcquire(AppPaths::ensureUserDataDir(m_info.internalName));
        if (m_dataDirLock)
            return true;

        const auto& displayName = m_info.displayName.empty() ? m_info.internalName : m_info.displayName;
        const auto text = LocaleManager::instance().translate("app.alreadyRunning", { { "name", displayName } });
        log::error("{}", text);
        std::cerr << text << '\n';
        return false;
    }

    void StapikApplication::on_startup()
    {
        Application::on_startup();

        LocaleManager::instance();

        const auto& displayName = m_info.displayName.empty() ? m_info.internalName : m_info.displayName;
        Glib::set_application_name(Glib::ustring(displayName));

        applyTheme();
        m_themeConnection = ThemeManager::instance().signalThemeChanged().connect([this] { applyTheme(); });

        StandardMenu::installShortcuts(*this);
        add_action(QUIT_ACTION, [this] { requestQuit(); });

        if (m_options.checkForUpdates)
            startUpdateCheck();
    }

    void StapikApplication::requestQuit()
    {
        const auto windows = get_windows();
        if (windows.empty())
        {
            quit();
            return;
        }

        for (auto* window : windows)
            window->close();
    }

    std::string StapikApplication::windowStateKey(const Gtk::Window&)
    {
        // The window is already on the list when window_added is emitted, so the first one is alone there.
        return get_windows().size() == 1 ? MAIN_WINDOW_STATE_KEY : std::string();
    }

    void StapikApplication::applyTheme()
    {
        m_styleProvider.apply(ThemeManager::instance().themeId());
    }

    void StapikApplication::startUpdateCheck()
    {
        m_updateChecker = update::UpdateChecker::forApp(m_info, AppContext::instance().settings());
        if (!m_updateChecker)
            return;

        m_updateConnection = m_updateChecker->signalUpdateAvailable().connect([this](const update::ReleaseInfo& release) { announceUpdate(release); });
        m_updateChecker->check();
    }

    void StapikApplication::announceUpdate(const update::ReleaseInfo& release)
    {
        auto* window = primaryWindow();
        if (window == nullptr)
        {
            log::debug("Version {} is available, but there is no window to announce it in", release.version.toString());
            return;
        }

        showUpdateAvailableDialog(*window, m_info, release);
    }

    Gtk::Window* StapikApplication::primaryWindow()
    {
        if (auto* active = get_active_window())
            return active;

        const auto windows = get_windows();
        return windows.empty() ? nullptr : windows.front();
    }

    void StapikApplication::onSecondInstance(const std::vector<std::filesystem::path>& files)
    {
        using Flags = Gio::Application::Flags;

        if (!files.empty() && (get_flags() & Flags::HANDLES_OPEN) == Flags::HANDLES_OPEN)
        {
            std::vector<Glib::RefPtr<Gio::File>> toOpen;
            toOpen.reserve(files.size());
            for (const auto& file : files)
                toOpen.push_back(Gio::File::create_for_path(storage::pathText(file)));

            open(toOpen);
        }
        else
        {
            activate();
        }

        if (auto* window = primaryWindow())
        {
            window->unminimize();
            window->present();
        }
    }

    void StapikApplication::onWindowAdded(Gtk::Window& window)
    {
        installQuitAction(window);

        if (!m_options.rememberWindowState || m_commandLine.safeMode)
            return;

        const auto key = windowStateKey(window);
        if (key.empty())
            return;

        restoreWindowState(window, key);

        window.signal_close_request().connect([&window, key]
        {
            saveWindowState(window, key);
            return false;
        }, false);
    }

    void StapikApplication::installQuitAction(Gtk::Window& window)
    {
        auto* actionMap = dynamic_cast<ActionMap*>(&window);
        if (actionMap == nullptr || actionMap->lookup_action(QUIT_ACTION))
            return;

        actionMap->add_action(QUIT_ACTION, [this] { requestQuit(); });
    }

    void StapikApplication::restoreWindowState(Gtk::Window& window, const std::string& key)
    {
        const auto state = ui::WindowState::load(AppContext::instance().settings(), key);
        if (!state)
            return;

        if (state->hasSize())
            window.set_default_size(state->width, state->height);

        if (state->maximized)
        {
            window.maximize();
            return;
        }

        if (state->hasPosition())
            ui::restoreWindowPositionWhenMapped(window, *state->position);
    }

    void StapikApplication::saveWindowState(Gtk::Window& window, const std::string& key)
    {
        auto& store = AppContext::instance().settings();

        ui::WindowState state = ui::WindowState::load(store, key).value_or(ui::WindowState{});
        state.maximized = window.is_maximized();

        if (!state.maximized && !window.is_fullscreen())
        {
            state.width = window.get_width();
            state.height = window.get_height();

            if (const auto position = ui::currentWindowPosition(window))
                state.position = position;
        }

        if (!state.save(store, key))
            log::warning("Cannot persist the state of window '{}'", key);
    }
}
