#include "TutorialWindow.hpp"

#include "DialogUtils.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/tutorial/TutorialLoader.hpp"
#include "stapik/ui/widget/EmptyState.hpp"
#include "stapik/ui/widget/MarkdownView.hpp"
#include "stapik/ui/window/PanedPosition.hpp"

#include <giomm/liststore.h>
#include <glibmm/object.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listview.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treeexpander.h>
#include <gtkmm/treelistmodel.h>
#include <sigc++/scoped_connection.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr int WINDOW_WIDTH = 820;
    constexpr int WINDOW_HEIGHT = 540;
    constexpr int CONTENTS_WIDTH = 230;
    constexpr int TOOLBAR_SPACING = 6;
    constexpr int TOOLBAR_MARGIN = 6;
    constexpr auto CONTENTS_PANE_KEY = "window.panes.tutorial";
    constexpr auto EMPTY_ICON = "help-browser-symbolic";
    constexpr auto EMPTY_TITLE_KEY = "dialog.tutorial.empty";
    constexpr auto CHAPTER_LINK_PREFIX = '#';
    constexpr std::array WEB_LINK_PREFIXES{ "http://", "https://", "mailto:" };

    bool isWebLink(const std::string& target)
    {
        return std::ranges::any_of(WEB_LINK_PREFIXES, [&target](const std::string_view prefix) { return target.starts_with(prefix); });
    }

    class ChapterItem : public Glib::Object
    {
    public:
        using Store = Gio::ListStore<ChapterItem>;

        static Glib::RefPtr<ChapterItem> create(const stapik::tutorial::Chapter& chapter)
        {
            return Glib::make_refptr_for_instance<ChapterItem>(new ChapterItem(chapter));
        }

        static Glib::RefPtr<Store> createStore(const std::vector<stapik::tutorial::Chapter>& chapters)
        {
            auto store = Store::create();
            for (const auto& chapter : chapters)
                store->append(create(chapter));
            return store;
        }

        [[nodiscard]] const std::string& id() const
        {
            return m_id;
        }

        [[nodiscard]] const std::string& title() const
        {
            return m_title;
        }

        [[nodiscard]] Glib::RefPtr<Store> children() const
        {
            return m_children;
        }

    private:
        explicit ChapterItem(const stapik::tutorial::Chapter& chapter) :
            m_id(chapter.id),
            m_title(chapter.title),
            m_children(chapter.children.empty() ? nullptr : createStore(chapter.children))
        {}

        std::string m_id;
        std::string m_title;
        Glib::RefPtr<Store> m_children;
    };

    class TutorialWindow final : public Gtk::Window
    {
    public:
        TutorialWindow(Window& parent, std::filesystem::path tutorialDirectory, std::string chapterId);
        ~TutorialWindow() override;

        TutorialWindow(const TutorialWindow&) = delete;
        TutorialWindow& operator=(const TutorialWindow&) = delete;

        void open(const std::string& chapterId);

    private:
        Gtk::Box m_root{ Gtk::Orientation::VERTICAL };
        Gtk::Box m_toolbar{ Gtk::Orientation::HORIZONTAL, TOOLBAR_SPACING };
        Gtk::ToggleButton m_contentsToggle;
        Gtk::Button m_previousButton;
        Gtk::Button m_nextButton;
        Gtk::Paned m_paned{ Gtk::Orientation::HORIZONTAL };
        Gtk::ScrolledWindow m_contentsScroller;
        Gtk::ListView m_contentsView;
        stapik::ui::MarkdownView m_page;
        EmptyState m_emptyState{ EMPTY_ICON, EMPTY_TITLE_KEY };
        Glib::RefPtr<Gtk::TreeListModel> m_treeModel;
        Glib::RefPtr<Gtk::SingleSelection> m_selection;
        std::filesystem::path m_tutorialDirectory;
        stapik::tutorial::Tutorial m_tutorial;
        std::string m_currentId;
        bool m_selectingInTree = false;
        sigc::scoped_connection m_localeConnection;

        void initToolbar();
        void initContents();
        void initLayout();
        void reload();
        void retranslate();
        void rebuildContents();
        void showChapter(const std::string& chapterId);
        void showNeighbour(const stapik::tutorial::Chapter* chapter);
        void selectInTree(const std::string& chapterId);
        void onTreeSelectionChanged();
        void onLinkActivated(const std::string& target);
        void updateNavigation();
        [[nodiscard]] static Glib::RefPtr<Gtk::SignalListItemFactory> createRowFactory();
    };

    TutorialWindow* g_openWindow = nullptr;

    TutorialWindow::TutorialWindow(Window& parent, std::filesystem::path tutorialDirectory, std::string chapterId) :
        m_tutorialDirectory(std::move(tutorialDirectory)),
        m_currentId(std::move(chapterId))
    {
        g_openWindow = this;

        add_css_class("stapik-tutorial-window");
        set_transient_for(parent);
        set_destroy_with_parent(true);
        set_default_size(WINDOW_WIDTH, WINDOW_HEIGHT);

        initToolbar();
        initContents();
        initLayout();

        m_page.signalLinkActivated().connect([this](const std::string& target) { onLinkActivated(target); });
        m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { reload(); });

        reload();
    }

    TutorialWindow::~TutorialWindow()
    {
        g_openWindow = nullptr;
    }

    void TutorialWindow::open(const std::string& chapterId)
    {
        if (!chapterId.empty())
            showChapter(chapterId);
        present();
    }

    void TutorialWindow::initToolbar()
    {
        m_contentsToggle.set_active(true);
        m_contentsToggle.signal_toggled().connect([this] { m_contentsScroller.set_visible(m_contentsToggle.get_active()); });

        m_previousButton.signal_clicked().connect([this] { showNeighbour(m_tutorial.previous(m_currentId)); });
        m_nextButton.signal_clicked().connect([this] { showNeighbour(m_tutorial.next(m_currentId)); });

        m_toolbar.set_margin(TOOLBAR_MARGIN);
        m_toolbar.append(m_contentsToggle);
        m_toolbar.append(m_previousButton);
        m_toolbar.append(m_nextButton);
    }

    void TutorialWindow::initContents()
    {
        m_contentsView.set_factory(createRowFactory());
        m_contentsView.add_css_class("stapik-tutorial-contents");

        m_contentsScroller.set_child(m_contentsView);
        m_contentsScroller.set_vexpand(true);
    }

    void TutorialWindow::initLayout()
    {
        m_paned.set_start_child(m_contentsScroller);
        m_paned.set_end_child(m_page);
        m_paned.set_resize_start_child(false);
        m_paned.set_shrink_start_child(false);
        m_paned.set_position(CONTENTS_WIDTH);
        m_paned.set_vexpand(true);

        if (stapik::app::AppContext::isInitialized())
            stapik::ui::rememberPanedPosition(m_paned, stapik::app::AppContext::instance().settings(), CONTENTS_PANE_KEY);

        m_root.append(m_toolbar);
        m_root.append(m_paned);
        m_root.append(m_emptyState);
        set_child(m_root);
    }

    Glib::RefPtr<Gtk::SignalListItemFactory> TutorialWindow::createRowFactory()
    {
        const auto factory = Gtk::SignalListItemFactory::create();

        factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& listItem)
        {
            auto* expander = Gtk::make_managed<Gtk::TreeExpander>();
            auto* label = Gtk::make_managed<Gtk::Label>();
            label->set_xalign(0.0F);
            label->set_ellipsize(Pango::EllipsizeMode::END);
            expander->set_child(*label);
            listItem->set_child(*expander);
        });

        factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& listItem)
        {
            const auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(listItem->get_item());
            auto* expander = dynamic_cast<Gtk::TreeExpander*>(listItem->get_child());
            if (!row || expander == nullptr)
                return;

            expander->set_list_row(row);
            const auto item = std::dynamic_pointer_cast<ChapterItem>(row->get_item());
            if (auto* label = dynamic_cast<Gtk::Label*>(expander->get_child()); label != nullptr && item)
                label->set_text(item->title());
        });

        return factory;
    }

    void TutorialWindow::reload()
    {
        m_tutorial = stapik::tutorial::loadTutorial(m_tutorialDirectory, LocaleManager::instance().languageCode());

        retranslate();
        rebuildContents();

        const bool hasChapters = !m_tutorial.empty();
        m_toolbar.set_visible(hasChapters);
        m_paned.set_visible(hasChapters);
        m_emptyState.set_visible(!hasChapters);

        const auto* chapter = m_tutorial.find(m_currentId);
        if (chapter == nullptr)
            chapter = m_tutorial.first();
        if (chapter != nullptr)
            showChapter(chapter->id);
    }

    void TutorialWindow::retranslate()
    {
        const auto& loc = LocaleManager::instance();
        const auto& info = stapik::app::AppContext::instance().info();

        set_title(loc.translate("dialog.tutorial.title", { { "name", info.displayName.empty() ? info.internalName : info.displayName } }));
        m_contentsToggle.set_label(loc.translate("dialog.tutorial.contents"));
        m_previousButton.set_label(loc.translate("dialog.tutorial.previous"));
        m_nextButton.set_label(loc.translate("dialog.tutorial.next"));
    }

    void TutorialWindow::rebuildContents()
    {
        const auto selecting = std::exchange(m_selectingInTree, true);

        const auto createChildren = [](const Glib::RefPtr<ObjectBase>& object) -> Glib::RefPtr<Gio::ListModel>
        {
            const auto item = std::dynamic_pointer_cast<ChapterItem>(object);
            return item ? item->children() : nullptr;
        };

        m_treeModel = Gtk::TreeListModel::create(ChapterItem::createStore(m_tutorial.chapters), createChildren, false, true);
        m_selection = Gtk::SingleSelection::create(m_treeModel);
        m_selection->property_selected().signal_changed().connect([this] { onTreeSelectionChanged(); });
        m_contentsView.set_model(m_selection);

        m_selectingInTree = selecting;
    }

    void TutorialWindow::showChapter(const std::string& chapterId)
    {
        const auto* chapter = m_tutorial.find(chapterId);
        if (chapter == nullptr)
        {
            stapik::log::warning("Tutorial chapter '{}' does not exist", chapterId);
            return;
        }

        m_currentId = chapter->id;
        m_page.setDocument(chapter->content, chapter->directory);
        selectInTree(chapter->id);
        updateNavigation();
    }

    void TutorialWindow::showNeighbour(const stapik::tutorial::Chapter* chapter)
    {
        if (chapter != nullptr)
            showChapter(chapter->id);
    }

    void TutorialWindow::selectInTree(const std::string& chapterId)
    {
        const auto selecting = std::exchange(m_selectingInTree, true);

        for (guint position = 0; position < m_treeModel->get_n_items(); ++position)
        {
            const auto row = m_treeModel->get_row(position);
            const auto item = row ? std::dynamic_pointer_cast<ChapterItem>(row->get_item()) : nullptr;
            if (!item)
                continue;

            if (item->id() == chapterId)
            {
                m_selection->set_selected(position);
                break;
            }

            if (chapterId.starts_with(item->id() + "/"))
                row->set_expanded(true);
        }

        m_selectingInTree = selecting;
    }

    void TutorialWindow::onTreeSelectionChanged()
    {
        if (m_selectingInTree)
            return;

        const auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(m_selection->get_selected_item());
        if (const auto item = row ? std::dynamic_pointer_cast<ChapterItem>(row->get_item()) : nullptr)
            showChapter(item->id());
    }

    void TutorialWindow::onLinkActivated(const std::string& target)
    {
        if (target.starts_with(CHAPTER_LINK_PREFIX))
            showChapter(target.substr(1));
        else if (isWebLink(target))
            openInBrowser(target);
        else
            stapik::log::warning("Tutorial link '{}' is neither a chapter nor a web address", target);
    }

    void TutorialWindow::updateNavigation()
    {
        m_previousButton.set_sensitive(m_tutorial.previous(m_currentId) != nullptr);
        m_nextButton.set_sensitive(m_tutorial.next(m_currentId) != nullptr);
    }
}

void showTutorialWindow(Gtk::Window& parent, const std::filesystem::path& tutorialDirectory, const std::string& chapterId)
{
    if (g_openWindow != nullptr)
    {
        g_openWindow->open(chapterId);
        return;
    }

    auto* window = new TutorialWindow(parent, tutorialDirectory, chapterId);
    window->signal_hide().connect([window] { delete window; });
    window->present();
}
