#pragma once

#include "stapik/markdown/MarkdownDocument.hpp"

#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/texttag.h>
#include <gtkmm/textview.h>
#include <sigc++/signal.h>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace stapik::ui
{
    class MarkdownView : public Gtk::ScrolledWindow
    {
    public:
        MarkdownView();

        MarkdownView(const MarkdownView&) = delete;
        MarkdownView& operator=(const MarkdownView&) = delete;

        void setDocument(const markdown::Document& document, const std::filesystem::path& imageDirectory);

        sigc::signal<void(const std::string&)>& signalLinkActivated();

    private:
        using TagPtr = Glib::RefPtr<Gtk::TextTag>;

        struct LinkRange
        {
            int begin = 0;
            int end = 0;
            std::string target;
        };

        void initTags();
        void initLinkHandling();
        void appendBlock(const markdown::Block& block, const std::filesystem::path& imageDirectory);
        void appendSpans(const std::vector<markdown::Span>& spans);
        void appendSpan(const markdown::Span& span);
        void appendText(const std::string& text) const;
        bool appendImage(const markdown::Block& block, const std::filesystem::path& imageDirectory) const;
        [[nodiscard]] TagPtr headingTag(int level) const;
        [[nodiscard]] TagPtr listItemTag(const markdown::Block& block, const std::string& marker);
        void applyTag(const TagPtr& tag, int begin, int end) const;
        void scrollToTop();
        void updateCursor(double x, double y);
        void activateLinkAt(double x, double y);
        [[nodiscard]] const LinkRange* linkAt(double x, double y);
        [[nodiscard]] int endOffset() const;
        [[nodiscard]] int textWidth(const std::string& text);

        Gtk::TextView m_textView;
        Glib::RefPtr<Gtk::TextBuffer> m_buffer;
        std::vector<TagPtr> m_headingTags;
        TagPtr m_paragraphTag;
        TagPtr m_quoteTag;
        TagPtr m_codeBlockTag;
        TagPtr m_ruleTag;
        TagPtr m_boldTag;
        TagPtr m_italicTag;
        TagPtr m_codeTag;
        TagPtr m_linkTag;
        std::map<std::pair<int, int>, TagPtr> m_listItemTags;
        std::vector<LinkRange> m_links;
        bool m_overLink = false;
        sigc::signal<void(const std::string&)> m_signalLinkActivated;
    };
}
