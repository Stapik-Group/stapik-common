#include "LogViewerDialog.hpp"

#include "DialogUtils.hpp"
#include "StapikDialog.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/LogReport.hpp"
#include "stapik/log/MemoryLog.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"

#include <giomm/file.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glibmm/error.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/textmark.h>
#include <gtkmm/textview.h>

#include <array>
#include <string>
#include <vector>

namespace
{
    using stapik::log::Level;

    constexpr int DIALOG_WIDTH = 720;
    constexpr int DIALOG_HEIGHT = 480;
    constexpr int ROW_SPACING = 8;
    constexpr int TEXT_MARGIN = 6;

    constexpr std::array LEVELS{Level::Debug, Level::Info, Level::Warning, Level::Error};
    constexpr std::array LEVEL_KEYS{
        "dialog.log.level.debug",
        "dialog.log.level.info",
        "dialog.log.level.warning",
        "dialog.log.level.error"};

    std::string validUtf8(const std::string& text)
    {
        gchar* valid = g_utf8_make_valid(text.c_str(), static_cast<gssize>(text.size()));
        std::string result(valid);
        g_free(valid);
        return result;
    }

    std::vector<stapik::log::LogRecord> loggedRecords()
    {
        const auto memoryLog = stapik::log::memoryLog();
        return memoryLog ? memoryLog->snapshot() : std::vector<stapik::log::LogRecord>{};
    }

    class LogViewerDialog final : public StapikDialog
    {
    public:
        explicit LogViewerDialog(Window& parent);

    private:
        Gtk::Box m_toolbar{Gtk::Orientation::HORIZONTAL, ROW_SPACING};
        Gtk::Label m_levelLabel;
        Gtk::DropDown m_levelDropDown;
        Gtk::Button m_refreshButton;
        Gtk::Box m_toolbarSpacer;
        Gtk::Button m_copyButton;
        Gtk::Button m_saveButton;
        Gtk::ScrolledWindow m_scroller;
        Gtk::TextView m_textView;
        Gtk::Label m_statusLabel;
        Glib::RefPtr<Gtk::TextMark> m_endMark;

        void initToolbar();
        void initTextView();
        void reload();
        void copyReport();
        void saveReport();
        void writeReport(const Glib::RefPtr<Gtk::FileDialog>& fileDialog, const Glib::RefPtr<Gio::AsyncResult>& result);
        void showStatus(const std::string& text);
        [[nodiscard]] Level selectedLevel() const;
        [[nodiscard]] std::string buildReport() const;
        static std::string defaultFileName() ;
    };

    LogViewerDialog::LogViewerDialog(Window& parent) :
        StapikDialog(parent, LocaleManager::instance().translate("dialog.log.title"))
    {
        add_css_class("stapik-log-dialog");
        set_default_size(DIALOG_WIDTH, DIALOG_HEIGHT);

        initToolbar();
        initTextView();

        m_statusLabel.set_halign(Gtk::Align::START);
        m_statusLabel.set_wrap(true);
        m_statusLabel.add_css_class("dim-label");

        contentBox().append(m_toolbar);
        contentBox().append(m_scroller);
        contentBox().append(m_statusLabel);

        add_button(LocaleManager::instance().translate("dialog.button.close"), Gtk::ResponseType::CLOSE);

        reload();
    }

    void LogViewerDialog::initToolbar()
    {
        const auto& loc = LocaleManager::instance();

        std::vector<Glib::ustring> levelLabels;
        for (const auto* key : LEVEL_KEYS)
            levelLabels.emplace_back(loc.translate(key));

        m_levelLabel.set_text(loc.translate("dialog.log.level.label"));
        m_levelDropDown.set_model(Gtk::StringList::create(levelLabels));
        m_levelDropDown.set_selected(0);
        m_levelDropDown.property_selected().signal_changed().connect([this] { reload(); });

        m_refreshButton.set_label(loc.translate("dialog.log.refresh"));
        m_refreshButton.signal_clicked().connect([this] { reload(); });

        m_toolbarSpacer.set_hexpand(true);

        m_copyButton.set_label(loc.translate("dialog.log.copy"));
        m_copyButton.signal_clicked().connect([this] { copyReport(); });

        m_saveButton.set_label(loc.translate("dialog.log.save"));
        m_saveButton.signal_clicked().connect([this] { saveReport(); });

        m_toolbar.append(m_levelLabel);
        m_toolbar.append(m_levelDropDown);
        m_toolbar.append(m_refreshButton);
        m_toolbar.append(m_toolbarSpacer);
        m_toolbar.append(m_copyButton);
        m_toolbar.append(m_saveButton);
    }

    void LogViewerDialog::initTextView()
    {
        m_textView.set_editable(false);
        m_textView.set_cursor_visible(false);
        m_textView.set_monospace(true);
        m_textView.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
        m_textView.set_left_margin(TEXT_MARGIN);
        m_textView.set_right_margin(TEXT_MARGIN);

        const auto buffer = m_textView.get_buffer();
        m_endMark = buffer->create_mark(buffer->end(), false);

        m_scroller.set_child(m_textView);
        m_scroller.set_expand(true);
    }

    void LogViewerDialog::reload()
    {
        const auto records = stapik::log::filterByLevel(loggedRecords(), selectedLevel());
        const auto text = stapik::log::formatLogText(records);

        const auto buffer = m_textView.get_buffer();
        buffer->set_text(text.empty() ? LocaleManager::instance().translate("dialog.log.empty") : validUtf8(text));
        buffer->move_mark(m_endMark, buffer->end());
        m_textView.scroll_to(m_endMark);

        showStatus({});
    }

    void LogViewerDialog::copyReport()
    {
        get_clipboard()->set_text(buildReport());
        showStatus(LocaleManager::instance().translate("dialog.log.copied"));
    }

    void LogViewerDialog::saveReport()
    {
        const auto fileDialog = Gtk::FileDialog::create();
        fileDialog->set_title(LocaleManager::instance().translate("dialog.log.save.title"));
        fileDialog->set_initial_name(defaultFileName());
        fileDialog->save(*this, [this, fileDialog](const Glib::RefPtr<Gio::AsyncResult>& result)
        {
            writeReport(fileDialog, result);
        });
    }

    void LogViewerDialog::writeReport(const Glib::RefPtr<Gtk::FileDialog>& fileDialog, const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        const auto& loc = LocaleManager::instance();

        Glib::RefPtr<Gio::File> file;
        try
        {
            file = fileDialog->save_finish(result);
        }
        catch (const Glib::Error& error)
        {
            if (g_error_matches(error.gobj(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) ||
                g_error_matches(error.gobj(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
                return;

            showStatus(loc.translate("dialog.log.saveFailed", {{"error", error.what()}}));
            return;
        }

        const auto path = file ? file->get_path() : std::string();
        if (path.empty() || !stapik::storage::writeFileAtomically(stapik::storage::pathFromText(path), buildReport()))
        {
            showStatus(loc.translate("dialog.log.saveFailed", {{"error", path}}));
            return;
        }

        showStatus(loc.translate("dialog.log.saved", {{"path", path}}));
    }

    void LogViewerDialog::showStatus(const std::string& text)
    {
        m_statusLabel.set_text(text);
        m_statusLabel.set_visible(!text.empty());
    }

    Level LogViewerDialog::selectedLevel() const
    {
        const auto index = m_levelDropDown.get_selected();
        return index < LEVELS.size() ? LEVELS[index] : Level::Debug;
    }

    std::string LogViewerDialog::buildReport() const
    {
        const auto& info = stapik::app::AppContext::instance().info();
        const stapik::log::ReportHeader header{
            .application = info.displayName.empty() ? info.internalName : info.displayName,
            .version = info.version};

        return validUtf8(stapik::log::formatBugReport(header, selectedLevel(), loggedRecords()));
    }

    std::string LogViewerDialog::defaultFileName()
    {
        const auto& name = stapik::app::AppContext::instance().info().internalName;
        return (name.empty() ? "stapik" : name) + "-log.txt";
    }
}

void showLogViewerDialog(Gtk::Window& parent)
{
    showAutoDeletingDialog<LogViewerDialog>(parent);
}
