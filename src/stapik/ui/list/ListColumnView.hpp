#pragma once

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/text/Collation.hpp"
#include "stapik/text/TextFolding.hpp"

#include <giomm/liststore.h>
#include <glibmm/object.h>
#include <glibmm/wrap.h>
#include <gtk/gtk.h>
#include <gtkmm/columnview.h>
#include <gtkmm/columnviewcolumn.h>
#include <gtkmm/filter.h>
#include <gtkmm/filterlistmodel.h>
#include <gtkmm/label.h>
#include <gtkmm/listitem.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/sorter.h>
#include <gtkmm/sortlistmodel.h>
#include <sigc++/scoped_connection.h>
#include <sigc++/signal.h>

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace stapik::ui
{
    template<std::derived_from<Glib::Object> ItemType>
    class ListColumnView : public Gtk::ScrolledWindow
    {
    public:
        using ItemPtr = Glib::RefPtr<ItemType>;
        using TextProvider = std::function<std::string(const ItemType&)>;
        using Predicate = std::function<bool(const ItemType&)>;

        struct Column
        {
            std::string titleKey;
            TextProvider text;
            bool expand = false;
        };

        ListColumnView()
        {
            m_view.set_model(m_selection);
            m_view.signal_activate().connect([this](const guint position) { activate(position); });
            m_selection->property_selected().signal_changed().connect([this] { m_signalSelectionChanged.emit(); });
            m_localeConnection = LocaleManager::instance().signalLocaleChanged().connect([this] { retranslate(); });

            set_hexpand(true);
            set_vexpand(true);
            set_child(m_view);
        }

        ListColumnView(const ListColumnView&) = delete;
        ListColumnView& operator=(const ListColumnView&) = delete;

        void addColumn(Column column)
        {
            auto viewColumn = Gtk::ColumnViewColumn::create(LocaleManager::instance().translate(column.titleKey), createFactory(column.text));
            viewColumn->set_expand(column.expand);
            viewColumn->set_resizable(true);
            viewColumn->set_sorter(createSorter(std::move(column.text)));

            m_view.append_column(viewColumn);
            m_columns.push_back({ viewColumn, std::move(column.titleKey) });
        }

        void setItems(const std::vector<ItemPtr>& items)
        {
            m_store->splice(0, m_store->get_n_items(), items);
        }

        void append(const ItemPtr& item)
        {
            m_store->append(item);
        }

        void clear()
        {
            m_store->remove_all();
        }

        void setPredicate(Predicate predicate)
        {
            m_predicate = std::move(predicate);
            gtk_filter_changed(m_filter->gobj(), GTK_FILTER_CHANGE_DIFFERENT);
        }

        void setSearchText(TextProvider searchText)
        {
            m_searchText = std::move(searchText);
            gtk_filter_changed(m_filter->gobj(), GTK_FILTER_CHANGE_DIFFERENT);
        }

        void setQuery(const std::string& query)
        {
            m_foldedQuery = text::foldForSearch(query);
            gtk_filter_changed(m_filter->gobj(), GTK_FILTER_CHANGE_DIFFERENT);
        }

        [[nodiscard]] ItemPtr selectedItem() const
        {
            return std::dynamic_pointer_cast<ItemType>(m_selection->get_selected_item());
        }

        [[nodiscard]] unsigned int totalCount() const
        {
            return m_store->get_n_items();
        }

        [[nodiscard]] unsigned int visibleCount() const
        {
            return m_sorted->get_n_items();
        }

        [[nodiscard]] Gtk::ColumnView& columnView()
        {
            return m_view;
        }

        sigc::signal<void(const ItemPtr&)>& signalActivated()
        {
            return m_signalActivated;
        }

        sigc::signal<void()>& signalSelectionChanged()
        {
            return m_signalSelectionChanged;
        }

    private:
        struct TitledColumn
        {
            Glib::RefPtr<Gtk::ColumnViewColumn> column;
            std::string titleKey;
        };

        static Glib::RefPtr<Gtk::SignalListItemFactory> createFactory(const TextProvider& text)
        {
            const auto factory = Gtk::SignalListItemFactory::create();
            factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& listItem)
            {
                auto* label = Gtk::make_managed<Gtk::Label>();
                label->set_xalign(0.0F);
                label->set_ellipsize(Pango::EllipsizeMode::END);
                listItem->set_child(*label);
            });
            factory->signal_bind().connect([text](const Glib::RefPtr<Gtk::ListItem>& listItem)
            {
                const auto item = std::dynamic_pointer_cast<ItemType>(listItem->get_item());
                auto* label = dynamic_cast<Gtk::Label*>(listItem->get_child());
                if (item && label)
                    label->set_text(text(*item));
            });
            return factory;
        }

        static const ItemType* itemOf(const void* object)
        {
            auto const* wrapped = Glib::wrap_auto(G_OBJECT(const_cast<void*>(object)), false);
            return dynamic_cast<const ItemType*>(wrapped);
        }

        static Glib::RefPtr<Gtk::Sorter> createSorter(TextProvider text)
        {
            const auto compare = [](const gconstpointer first, gconstpointer second, gpointer data) -> int
            {
                const auto& textOf = *static_cast<const TextProvider*>(data);
                const auto* firstItem = itemOf(first);
                const auto* secondItem = itemOf(second);
                if (firstItem == nullptr || secondItem == nullptr)
                    return 0;

                const auto order = text::compareText(textOf(*firstItem), textOf(*secondItem), LocaleManager::instance().languageCode());
                return std::clamp(order, -1, 1);
            };
            const auto release = [](gpointer data) { delete static_cast<TextProvider*>(data); };

            return Glib::wrap(GTK_SORTER(gtk_custom_sorter_new(compare, new TextProvider(std::move(text)), release)));
        }

        Glib::RefPtr<Gtk::Filter> createFilter()
        {
            const auto accepts = [](const gpointer object, gpointer data) -> gboolean
            {
                return static_cast<const ListColumnView*>(data)->matches(itemOf(object));
            };

            return Glib::wrap(GTK_FILTER(gtk_custom_filter_new(accepts, this, nullptr)));
        }

        bool matches(const ItemType* item) const
        {
            if (item == nullptr)
                return false;

            if (m_predicate && !m_predicate(*item))
                return false;

            return m_foldedQuery.empty() || !m_searchText || text::foldForSearch(m_searchText(*item)).find(m_foldedQuery) != std::string::npos;
        }

        void activate(const guint position)
        {
            if (const auto item = std::dynamic_pointer_cast<ItemType>(m_sorted->get_object(position)))
                m_signalActivated.emit(item);
        }

        void retranslate()
        {
            for (const auto& [column, titleKey] : m_columns)
            {
                column->set_title(LocaleManager::instance().translate(titleKey));
                gtk_sorter_changed(column->get_sorter()->gobj(), GTK_SORTER_CHANGE_DIFFERENT);
            }
        }

        Gtk::ColumnView m_view;
        Glib::RefPtr<Gio::ListStore<ItemType>> m_store = Gio::ListStore<ItemType>::create();
        Glib::RefPtr<Gtk::Filter> m_filter = createFilter();
        Glib::RefPtr<Gtk::FilterListModel> m_filtered = Gtk::FilterListModel::create(m_store, m_filter);
        Glib::RefPtr<Gtk::SortListModel> m_sorted = Gtk::SortListModel::create(m_filtered, m_view.get_sorter());
        Glib::RefPtr<Gtk::SingleSelection> m_selection = Gtk::SingleSelection::create(m_sorted);
        std::vector<TitledColumn> m_columns;
        Predicate m_predicate;
        TextProvider m_searchText;
        std::string m_foldedQuery;
        sigc::signal<void(const ItemPtr&)> m_signalActivated;
        sigc::signal<void()> m_signalSelectionChanged;
        sigc::scoped_connection m_localeConnection;
    };
}
