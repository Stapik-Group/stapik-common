#include "StandardMenu.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/theme/FontScale.hpp"
#include "stapik/theme/FontSizeManager.hpp"
#include "stapik/theme/ThemeManager.hpp"
#include "stapik/ui/dialog/AboutDialog.hpp"
#include "stapik/ui/dialog/LogViewerDialog.hpp"
#include "stapik/ui/dialog/SettingsDialog.hpp"
#include "stapik/ui/dialog/TutorialWindow.hpp"

#include <algorithm>
#include <utility>

namespace
{
    constexpr int FONT_SCALE_STEP_PERCENT = 5;

    std::string actionLabel(const LocaleManager& loc, const std::string& baseKey, const std::string& description)
    {
        if (description.empty())
            return loc.translate(baseKey);

        return loc.translate(baseKey + ".described", { { "description", description } });
    }
}

StandardMenu::StandardMenu(Gtk::ApplicationWindow& window, const StandardMenuOptions &options) :
    m_window(window),
    m_options(options)
{
    initSettingsLayout();
    initSettingsAction();
    initAboutAction();
    initLogAction();
    initTutorialAction();

    if (m_options.undoStack != nullptr)
    {
        m_undoActions = std::make_unique<UndoActions>(m_window, *m_options.undoStack);
        m_undoConnection = m_options.undoStack->signalChanged().connect([this] { rebuild(); });
    }

    m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { rebuild(); });

    rebuild();
}

StandardMenu::~StandardMenu()
{
    m_localeConnection.disconnect();
    m_undoConnection.disconnect();
}

stapik::ui::SettingsLayout& StandardMenu::settings()
{
    return m_settings;
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
    application.set_accels_for_action("win.tutorial", { "F1" });
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

    if (m_options.settingsItem)
    {
        const auto settingsSection = Gio::Menu::create();
        settingsSection->append(loc.translate("menu.settings.open"), "win.settings");
        menu->append_section(settingsSection);
    }

    applyBuilders(Target::Settings, *menu);

    return menu;
}

Glib::RefPtr<Gio::Menu> StandardMenu::buildHelpMenu() const
{
    const auto& loc = LocaleManager::instance();
    const auto menu = Gio::Menu::create();

    if (!m_options.tutorialDirectory.empty())
    {
        const auto tutorialSection = Gio::Menu::create();
        tutorialSection->append(loc.translate("menu.help.tutorial"), "win.tutorial");
        menu->append_section(tutorialSection);
    }

    if (m_options.logItem)
    {
        const auto logSection = Gio::Menu::create();
        logSection->append(loc.translate("menu.help.log"), "win.viewLog");
        menu->append_section(logSection);
    }

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

void StandardMenu::initSettingsLayout()
{
    using stapik::ui::ChoiceOption;

    if (auto& locale = LocaleManager::instance(); !locale.languages().empty())
    {
        m_settings.addChoice<std::string>("menu.settings.language", locale.languageSetting(), [&locale]
        {
            std::vector<ChoiceOption<std::string>> options;
            for (const auto& language : locale.languages())
                options.push_back({ .value = language.code, .label = language.displayName });

            return options;
        });
    }

    if (m_options.themes != nullptr && !m_options.themes->themes().empty())
    {
        m_settings.addChoice<std::string>("menu.settings.theme", ThemeManager::instance().themeSetting(), [themes = m_options.themes]
        {
            const auto& loc = LocaleManager::instance();

            std::vector<ChoiceOption<std::string>> options;
            for (const auto& theme : themes->themes())
            {
                const auto label = loc.translate(theme.nameKey);
                options.push_back({ .value = theme.id, .label = label == theme.nameKey ? theme.id : label });
            }

            return options;
        });
    }

    m_settings.addNumber<int>("menu.settings.fontSize", FontSizeManager::instance().scaleSetting(), {
        .min = stapik::theme::MIN_FONT_SCALE_PERCENT,
        .max = stapik::theme::MAX_FONT_SCALE_PERCENT,
        .step = FONT_SCALE_STEP_PERCENT });
}

void StandardMenu::initSettingsAction() const
{
    if (!m_options.settingsItem)
        return;

    m_window.add_action("settings", [this] { showSettingsDialog(m_window, m_settings); });
}

void StandardMenu::initAboutAction() const
{
    if (!m_options.aboutItem)
        return;

    m_window.add_action("about", [this] { showAboutDialog(m_window); });
}

void StandardMenu::initLogAction() const
{
    if (!m_options.logItem)
        return;

    m_window.add_action("viewLog", [this] { showLogViewerDialog(m_window); });
}

void StandardMenu::initTutorialAction() const
{
    if (m_options.tutorialDirectory.empty())
        return;

    m_window.add_action("tutorial", [this] { showTutorialWindow(m_window, m_options.tutorialDirectory); });
}
