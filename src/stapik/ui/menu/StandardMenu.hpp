#pragma once

#include "RadioAction.hpp"
#include "UndoActions.hpp"

#include "stapik/command/UndoStack.hpp"
#include "stapik/locale/LanguageRegistry.hpp"
#include "stapik/theme/ThemeRegistry.hpp"

#include <giomm/menu.h>
#include <gtkmm/application.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/popovermenubar.h>
#include <sigc++/connection.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct StandardMenuOptions
{
    bool cloudItems = true;
    bool undoRedoItems = true;
    bool aboutItem = true;
    const stapik::theme::ThemeRegistry* themes = nullptr;
    stapik::command::UndoStack* undoStack = nullptr;
};

class StandardMenu
{
public:
    enum class Target
    {
        File,
        Edit,
        Settings,
        Help
    };

    using MenuBuilder = std::function<void(Gio::Menu& menu)>;

    explicit StandardMenu(Gtk::ApplicationWindow& window, const StandardMenuOptions &options = {});
    ~StandardMenu();

    StandardMenu(const StandardMenu&) = delete;
    StandardMenu& operator=(const StandardMenu&) = delete;

    void addToMenu(Target target, MenuBuilder builder);
    void addMenu(std::string titleKey, MenuBuilder builder);
    static void installShortcuts(Gtk::Application& application);
    void rebuild();

    [[nodiscard]] Gtk::PopoverMenuBar& menuBar();

private:
    struct TargetBuilder
    {
        Target target;
        MenuBuilder builder;
    };

    struct ExtraMenu
    {
        std::string titleKey;
        MenuBuilder builder;
    };

    Glib::RefPtr<Gio::Menu> buildFileMenu() const;
    Glib::RefPtr<Gio::Menu> buildEditMenu() const;
    Glib::RefPtr<Gio::Menu> buildSettingsMenu() const;
    Glib::RefPtr<Gio::Menu> buildHelpMenu() const;
    void applyBuilders(Target target, Gio::Menu& menu) const;
    void initLanguageAction();
    void initThemeAction();
    void initAboutAction() const;

    Gtk::ApplicationWindow& m_window;
    StandardMenuOptions m_options;
    std::vector<stapik::locale::LanguageInfo> m_languages;
    std::vector<TargetBuilder> m_targetBuilders;
    std::vector<ExtraMenu> m_extraMenus;
    std::unique_ptr<stapik::ui::RadioAction<std::string>> m_languageAction;
    std::unique_ptr<stapik::ui::RadioAction<std::string>> m_themeAction;
    std::unique_ptr<UndoActions> m_undoActions;
    Glib::RefPtr<Gio::Menu> m_menuModel;
    Gtk::PopoverMenuBar m_menuBar;
    sigc::connection m_localeConnection;
    sigc::connection m_themeConnection;
    sigc::connection m_undoConnection;
};
