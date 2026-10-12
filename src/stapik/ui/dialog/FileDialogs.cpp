#include "FileDialogs.hpp"

#include "LastDirectory.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"

#include <giomm/asyncresult.h>
#include <giomm/file.h>
#include <glibmm/error.h>
#include <gtk/gtk.h>
#include <gtkmm/filedialog.h>

#include <utility>

namespace
{
    using FinishFunction = Glib::RefPtr<Gio::File> (Gtk::FileDialog::*)(const Glib::RefPtr<Gio::AsyncResult>&);

    bool isDismissal(const Glib::Error& error)
    {
        return g_error_matches(error.gobj(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) ||
               g_error_matches(error.gobj(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED);
    }

    void fail(const FileFailedHandler& onFailed, const std::string& message)
    {
        if (onFailed)
            onFailed(message);
    }

    void rememberDirectory(const std::string& key, const std::filesystem::path& chosen)
    {
        if (key.empty() || !stapik::app::AppContext::isInitialized())
            return;

        if (const auto saved = stapik::ui::saveLastDirectory(stapik::app::AppContext::instance().settings(), key, chosen); !saved)
            stapik::log::warning("Cannot remember the directory of '{}': {}", stapik::storage::pathText(chosen), saved.error().message());
    }

    void handleResult(const Glib::RefPtr<Gtk::FileDialog>& dialog,
                      const Glib::RefPtr<Gio::AsyncResult>& result,
                      const FinishFunction finish,
                      const FileDialogOptions& options,
                      const FileChosenHandler& onChosen,
                      const FileFailedHandler& onFailed)
    {
        Glib::RefPtr<Gio::File> file;
        try
        {
            file = (*dialog.*finish)(result);
        }
        catch (const Glib::Error& error)
        {
            if (!isDismissal(error))
                fail(onFailed, error.what());
            return;
        }

        const auto path = file ? file->get_path() : std::string();
        if (path.empty())
        {
            fail(onFailed, {});
            return;
        }

        const auto chosen = stapik::storage::pathFromText(path);
        rememberDirectory(options.lastDirectoryKey, chosen);

        if (onChosen)
            onChosen(chosen);
    }

    Glib::RefPtr<Gtk::FileDialog> createDialog(const FileDialogOptions& options)
    {
        const auto dialog = Gtk::FileDialog::create();
        dialog->set_title(options.title);
        dialog->set_modal(true);
        if (!options.initialName.empty())
            dialog->set_initial_name(options.initialName);

        if (options.lastDirectoryKey.empty() || !stapik::app::AppContext::isInitialized())
            return dialog;

        if (const auto directory = stapik::ui::loadLastDirectory(stapik::app::AppContext::instance().settings(), options.lastDirectoryKey))
            dialog->set_initial_folder(Gio::File::create_for_path(stapik::storage::pathText(*directory)));
        return dialog;
    }
}

void showSaveFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->save(parent, [dialog, options, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::save_finish, options, onChosen, onFailed);
    });
}

void showOpenFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->open(parent, [dialog, options, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::open_finish, options, onChosen, onFailed);
    });
}

void showSelectFolderDialog(Gtk::Window& parent,
                            const FileDialogOptions& options,
                            FileChosenHandler onChosen,
                            FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->select_folder(parent, [dialog, options, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::select_folder_finish, options, onChosen, onFailed);
    });
}
