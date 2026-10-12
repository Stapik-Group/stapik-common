#include "MarkdownView.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"

#include <gdkmm/texture.h>
#include <giomm/file.h>
#include <glibmm/error.h>
#include <gtk/gtk.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <pangomm/layout.h>

#include <algorithm>
#include <array>
#include <format>

namespace
{
    constexpr int TEXT_MARGIN = 16;
    constexpr int BLOCK_SPACING = 8;
    constexpr int HEADING_SPACING_ABOVE = 12;
    constexpr int LIST_BASE_MARGIN = 12;
    constexpr int LIST_DEPTH_MARGIN = 20;
    constexpr int QUOTE_MARGIN = 24;
    constexpr double CODE_SCALE = 0.92;
    constexpr std::array HEADING_SCALES{ 1.6, 1.35, 1.15, 1.0 };
    constexpr std::array BULLETS{ "•", "◦", "▪" };
    constexpr auto RULE_TEXT = "· · ·";
    constexpr auto LINK_CURSOR = "pointer";
    constexpr auto TEXT_CURSOR = "text";

    std::string validUtf8(const std::string& text)
    {
        gchar* valid = g_utf8_make_valid(text.c_str(), static_cast<gssize>(text.size()));
        std::string result(valid);
        g_free(valid);
        return result;
    }

    std::string markerOf(const stapik::markdown::Block& block)
    {
        if (block.isNumberedListItem())
            return std::format("{}.", block.listNumber);

        return BULLETS[static_cast<std::size_t>(std::min<int>(block.listDepth, static_cast<int>(BULLETS.size()) - 1))];
    }
}

namespace stapik::ui
{
    MarkdownView::MarkdownView() :
        m_buffer(m_textView.get_buffer())
    {
        add_css_class("stapik-markdown-view");

        m_textView.set_editable(false);
        m_textView.set_cursor_visible(false);
        m_textView.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
        m_textView.set_left_margin(TEXT_MARGIN);
        m_textView.set_right_margin(TEXT_MARGIN);
        m_textView.set_top_margin(TEXT_MARGIN);
        m_textView.set_bottom_margin(TEXT_MARGIN);

        set_expand(true);
        set_child(m_textView);

        initTags();
        initLinkHandling();
    }

    sigc::signal<void(const std::string&)>& MarkdownView::signalLinkActivated()
    {
        return m_signalLinkActivated;
    }

    void MarkdownView::initTags()
    {
        for (const double level : HEADING_SCALES)
        {
            auto tag = m_buffer->create_tag();
            tag->set_property("weight", static_cast<int>(PANGO_WEIGHT_BOLD));
            tag->property_scale() = level;
            tag->property_pixels_above_lines() = HEADING_SPACING_ABOVE;
            tag->property_pixels_below_lines() = BLOCK_SPACING / 2;
            m_headingTags.push_back(tag);
        }

        m_paragraphTag = m_buffer->create_tag();
        m_paragraphTag->property_pixels_below_lines() = BLOCK_SPACING;

        m_quoteTag = m_buffer->create_tag();
        m_quoteTag->property_left_margin() = QUOTE_MARGIN;
        m_quoteTag->property_style() = Pango::Style::ITALIC;
        m_quoteTag->property_pixels_below_lines() = BLOCK_SPACING;

        m_codeBlockTag = m_buffer->create_tag();
        m_codeBlockTag->property_left_margin() = LIST_BASE_MARGIN;
        m_codeBlockTag->property_wrap_mode() = Gtk::WrapMode::NONE;
        m_codeBlockTag->property_pixels_below_lines() = BLOCK_SPACING;

        m_ruleTag = m_buffer->create_tag();
        m_ruleTag->property_justification() = Gtk::Justification::CENTER;
        m_ruleTag->property_pixels_below_lines() = BLOCK_SPACING;

        m_boldTag = m_buffer->create_tag();
        m_boldTag->set_property("weight", static_cast<int>(PANGO_WEIGHT_BOLD));

        m_italicTag = m_buffer->create_tag();
        m_italicTag->property_style() = Pango::Style::ITALIC;

        m_codeTag = m_buffer->create_tag();
        m_codeTag->property_family() = "monospace";
        m_codeTag->property_scale() = CODE_SCALE;

        m_linkTag = m_buffer->create_tag();
        m_linkTag->property_underline() = Pango::Underline::SINGLE;
    }

    void MarkdownView::initLinkHandling()
    {
        const auto click = Gtk::GestureClick::create();
        click->signal_released().connect([this](int, const double x, const double y) { activateLinkAt(x, y); });
        m_textView.add_controller(click);

        const auto motion = Gtk::EventControllerMotion::create();
        motion->signal_motion().connect([this](const double x, const double y) { updateCursor(x, y); });
        m_textView.add_controller(motion);
    }

    void MarkdownView::setDocument(const markdown::Document& document, const std::filesystem::path& imageDirectory)
    {
        m_links.clear();
        m_buffer->set_text("");

        for (const auto& block : document)
            appendBlock(block, imageDirectory);

        scrollToTop();
    }

    void MarkdownView::appendBlock(const markdown::Block& block, const std::filesystem::path& imageDirectory)
    {
        using markdown::BlockKind;

        const int begin = endOffset();
        std::vector<TagPtr> blockTags;

        switch (block.kind)
        {
        using enum BlockKind;
        case Heading:
            blockTags = { headingTag(block.headingLevel) };
            appendSpans(block.spans);
            break;
        case Paragraph:
            blockTags = { m_paragraphTag };
            appendSpans(block.spans);
            break;
        case ListItem:
        {
            const auto marker = markerOf(block);
            blockTags = { listItemTag(block, marker) };
            appendText(marker + " ");
            appendSpans(block.spans);
            break;
        }
        case Quote:
            blockTags = { m_quoteTag };
            appendSpans(block.spans);
            break;
        case CodeBlock:
            blockTags = { m_codeBlockTag };
            appendSpans(block.spans);
            break;
        case Image:
            blockTags = { m_paragraphTag };
            if (!appendImage(block, imageDirectory))
                appendSpans(block.spans);
            break;
        case Rule:
            blockTags = { m_ruleTag };
            appendText(RULE_TEXT);
            break;
        }

        appendText("\n");

        for (const auto& tag : blockTags)
            applyTag(tag, begin, endOffset());
    }

    void MarkdownView::appendSpans(const std::vector<markdown::Span>& spans)
    {
        for (const auto& span : spans)
            appendSpan(span);
    }

    void MarkdownView::appendSpan(const markdown::Span& span)
    {
        const int begin = endOffset();
        appendText(span.text);
        const int end = endOffset();

        if (span.bold)
            applyTag(m_boldTag, begin, end);
        if (span.italic)
            applyTag(m_italicTag, begin, end);
        if (span.code)
            applyTag(m_codeTag, begin, end);
        if (!span.link.empty())
        {
            applyTag(m_linkTag, begin, end);
            m_links.push_back({ .begin = begin, .end = end, .target = span.link });
        }
    }

    void MarkdownView::appendText(const std::string& text) const
    {
        m_buffer->insert(m_buffer->end(), validUtf8(text));
    }

    bool MarkdownView::appendImage(const markdown::Block& block, const std::filesystem::path& imageDirectory) const
    {
        const auto path = imageDirectory / storage::pathFromText(block.imagePath);

        try
        {
            const auto texture = Gdk::Texture::create_from_file(Gio::File::create_for_path(storage::pathText(path)));
            auto position = m_buffer->end();
            gtk_text_buffer_insert_paintable(m_buffer->gobj(), position.gobj(), GDK_PAINTABLE(texture->gobj()));
            return true;
        }
        catch (const Glib::Error& error)
        {
            log::warning("Cannot load image {}: {}", storage::pathText(path), error.what());
            return false;
        }
    }

    MarkdownView::TagPtr MarkdownView::headingTag(const int level) const
    {
        const auto index = static_cast<std::size_t>(std::clamp(level, 1, static_cast<int>(m_headingTags.size()))) - 1;
        return m_headingTags[index];
    }

    MarkdownView::TagPtr MarkdownView::listItemTag(const markdown::Block& block, const std::string& marker)
    {
        const int markerWidth = textWidth(marker + " ");
        const std::pair key{ block.listDepth, markerWidth };

        if (const auto cached = m_listItemTags.find(key); cached != m_listItemTags.end())
            return cached->second;

        auto tag = m_buffer->create_tag();
        tag->property_left_margin() = LIST_BASE_MARGIN + block.listDepth * LIST_DEPTH_MARGIN;
        tag->property_indent() = -markerWidth;
        tag->property_pixels_below_lines() = BLOCK_SPACING / 2;
        m_listItemTags.try_emplace(key, tag);
        return tag;
    }

    void MarkdownView::applyTag(const TagPtr& tag, const int begin, const int end) const
    {
        m_buffer->apply_tag(tag, m_buffer->get_iter_at_offset(begin), m_buffer->get_iter_at_offset(end));
    }

    void MarkdownView::scrollToTop()
    {
        get_vadjustment()->set_value(0);
    }

    void MarkdownView::updateCursor(const double x, const double y)
    {
        const bool overLink = linkAt(x, y) != nullptr;
        if (overLink == m_overLink)
            return;

        m_overLink = overLink;
        m_textView.set_cursor(Glib::ustring(overLink ? LINK_CURSOR : TEXT_CURSOR));
    }

    void MarkdownView::activateLinkAt(const double x, const double y)
    {
        if (m_buffer->get_has_selection())
            return;

        if (const auto* link = linkAt(x, y))
            m_signalLinkActivated.emit(link->target);
    }

    const MarkdownView::LinkRange* MarkdownView::linkAt(const double x, const double y)
    {
        int bufferX = 0;
        int bufferY = 0;
        m_textView.window_to_buffer_coords(Gtk::TextWindowType::WIDGET, static_cast<int>(x), static_cast<int>(y), bufferX, bufferY);

        Gtk::TextBuffer::iterator position;
        if (!m_textView.get_iter_at_location(position, bufferX, bufferY))
            return nullptr;

        const int offset = position.get_offset();
        const auto match = std::ranges::find_if(m_links, [offset](const LinkRange& link) { return offset >= link.begin && offset < link.end; });
        return match == m_links.end() ? nullptr : std::to_address(match);
    }

    int MarkdownView::endOffset() const
    {
        return m_buffer->end().get_offset();
    }

    int MarkdownView::textWidth(const std::string& text)
    {
        int width = 0;
        int height = 0;
        m_textView.create_pango_layout(validUtf8(text))->get_pixel_size(width, height);
        return width;
    }
}
