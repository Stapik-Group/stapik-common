#include "StandardMenu.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/theme/ThemeManager.hpp"
#include "stapik/ui/dialog/AboutDialog.hpp"

#include <algorithm>
#include <utility>

namespace
{
    std::string actionLabel(const LocaleManager& loc, const std::string& baseKey, const std::string& description)
    {
        if (description.empty())
            return loc.translate(baseKey);

        return loc.translate(baseKey + ".described", { { "description", description } });
    }
}

StandardMenu::StandardMenu(Gtk::ApplicationWindow& window, const StandardMenuOptions &options) :
    m_window(window),
    m_options(options),
    m_languages(LocaleManager::instance().languages())
{
    initLanguageAction();
    initThemeAction();
    initAboutAction();

    if (m_options.undoStack != nullptr)
    {
        m_undoActions = std::make_unique<UndoActions>(m_window, *m_options.undoStack);
        m_undoConnection = m_options.undoStack->signalChanged().connect([this] { rebuild(); });
    }

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this]
    {
        if (m_languageAction)
            m_languageAction->setValue(LocaleManager::instance().languageCode());

        rebuild();
    });

    if (m_themeAction)
    {
        m_themeConnection = ThemeManager::instance().signalThemeChanged().connect([this]
        {
            m_themeAction->setValue(ThemeManager::instance().themeId());
        });
    }

    rebuild();
}

StandardMenu::~StandardMenu()
{
    m_localeConnection.disconnect();
    m_themeConnection.disconnect();
    m_undoConnection.disconnect();
}

void StandardMenu::addToMenu(const Target target, MenuBuilder builder)
{
    m_targetBuilders.push_back({ .target = target, .builder = std::move(builder) });
    rebuild();
}

void StandardMenu::addMenu(std::string titleKey, MenuBuilder builder)
{
    m_extraMenus.push_back({ .titleKey = std::move(titleKey), .builder = std::move(builder) });
    rebuild();
}

void StandardMenu::installShortcuts(Gtk::Application& application)
{
    application.set_accels_for_action("win.undo", { "<Primary>z" });
    application.set_accels_for_action("win.redo", { "<Primary><Shift>z" });
    application.set_accels_for_action("win.quit", { "<Primary>q" });
}

Gtk::PopoverMenuBar& StandardMenu::menuBar()
{
    return m_menuBar;
}

void StandardMenu::rebuild()
{
    const auto& loc = LocaleManager::instance();

    m_menuModel = Gio::Menu::create();

    if (const auto fileMenu = buildFileMenu(); fileMenu->get_n_items() > 0)
        m_menuModel->append_submenu(loc.translate("menu.file"), fileMenu);

    if (const auto editMenu = buildEditMenu(); editMenu->get_n_items() > 0)
        m_menuModel->append_submenu(loc.translate("menu.edit"), editMenu);

    for (const auto&[titleKey, builder] : m_extraMenus)
    {
        const auto menu = Gio::Menu::create();
        builder(*menu);
        m_menuModel->append_submenu(loc.translate(titleKey), menu);
    }

    if (const auto settingsMenu = buildSettingsMenu(); settingsMenu->get_n_items() > 0)
        m_menuModel->append_submenu(loc.translate("menu.settings"), settingsMenu);

    if (const auto helpMenu = buildHelpMenu(); helpMenu->get_n_items() > 0)
        m_menuModel->append_submenu(loc.translate("menu.help"), helpMenu);

    m_menuBar.set_menu_model(m_menuModel);
}

Glib::RefPtr<Gio::Menu> StandardMenu::buildFileMenu() const
{
    const auto& loc = LocaleManager::instance();
    const auto menu = Gio::Menu::create();

    if (m_options.cloudItems)
    {
        const auto cloudSection = Gio::Menu::create();
        cloudSection->append(loc.translate("menu.file.connect"), "win.connect");
        cloudSection->append(loc.translate("menu.file.sync"), "win.sync");
        menu->append_section(cloudSection);
    }

    applyBuilders(Target::File, *menu);

    const auto quitSection = Gio::Menu::create();
    quitSection->append(loc.translate("menu.file.quit"), "win.quit");
    menu->append_section(quitSection);

    return menu;
}

Glib::RefPtr<Gio::Menu> StandardMenu::buildEditMenu() const
{
    const auto& loc = LocaleManager::instance();
    const auto menu = Gio::Menu::create();

    if (m_options.undoRedoItems)
    {
        const auto historySection = Gio::Menu::create();
        const auto* undoStack = m_options.undoStack;
        historySection->append(actionLabel(loc, "menu.edit.undo", undoStack != nullptr ? undoStack->undoDescription() : std::string()), "win.undo");
        historySection->append(actionLabel(loc, "menu.edit.redo", undoStack != nullptr ? undoStack->redoDescription() : std::string()), "win.redo");
        menu->append_section(historySection);
    }

    applyBuilders(Target::Edit, *menu);

    return menu;
}

Glib::RefPtr<Gio::Menu> StandardMenu::buildSettingsMenu() const
{
    const auto& loc = LocaleManager::instance();
    const auto menu = Gio::Menu::create();

    if (m_languageAction && !m_languages.empty())
    {
        const auto languageMenu = Gio::Menu::create();
        for (const auto& language : m_languages)
            languageMenu->append(language.displayName, m_languageAction->detailedAction(language.code));

        menu->append_submenu(loc.translate("menu.settings.language"), languageMenu);
    }

    if (m_themeAction)
    {
        const auto themeMenu = Gio::Menu::create();
        for (const auto& theme : m_options.themes->themes())
        {
            const auto label = loc.translate(theme.nameKey);
            themeMenu->append(label == theme.nameKey ? theme.id : label, m_themeAction->detailedAction(theme.id));
        }

        menu->append_submenu(loc.translate("menu.settings.theme"), themeMenu);
    }

    applyBuilders(Target::Settings, *menu);

    return menu;
}

Glib::RefPtr<Gio::Menu> StandardMenu::buildHelpMenu() const
{
    const auto& loc = LocaleManager::instance();
    const auto menu = Gio::Menu::create();

    if (m_options.aboutItem)
    {
        const auto aboutSection = Gio::Menu::create();
        aboutSection->append(loc.translate("menu.help.about"), "win.about");
        menu->append_section(aboutSection);
    }

    applyBuilders(Target::Help, *menu);

    return menu;
}

void StandardMenu::applyBuilders(const Target target, Gio::Menu& menu) const
{
    for (const auto&[targetBuilder, builder] : m_targetBuilders)
    {
        if (targetBuilder == target)
            builder(menu);
    }
}

void StandardMenu::initLanguageAction()
{
    std::vector<stapik::ui::RadioOption<std::string>> options;
    for (const auto& language : m_languages)
        options.push_back({.value = language.code, .id = language.code});

    if (options.empty())
        return;

    m_languageAction = std::make_unique<stapik::ui::RadioAction<std::string>>(
        m_window,
        "setLanguage",
        std::move(options),
        LocaleManager::instance().languageCode(),
        [](const std::string& code) { LocaleManager::instance().setLanguage(code); });
}

void StandardMenu::initThemeAction()
{
    if (m_options.themes == nullptr || m_options.themes->themes().empty())
        return;

    std::vector<stapik::ui::RadioOption<std::string>> options;
    for (const auto& theme : m_options.themes->themes())
        options.emplace_back(theme.id, theme.id);

    m_themeAction = std::make_unique<stapik::ui::RadioAction<std::string>>(
        m_window,
        "setTheme",
        std::move(options),
        ThemeManager::instance().themeId(),
        [](const std::string& id) { ThemeManager::instance().setThemeId(id); });
}

void StandardMenu::initAboutAction() const
{
    if (!m_options.aboutItem)
        return;

    m_window.add_action("about", [this] { showAboutDialog(m_window); });
}
