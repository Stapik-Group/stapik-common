#include "FileDialogs.hpp"

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

    void handleResult(const Glib::RefPtr<Gtk::FileDialog>& dialog,
                      const Glib::RefPtr<Gio::AsyncResult>& result,
                      const FinishFunction finish,
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

        if (onChosen)
            onChosen(stapik::storage::pathFromText(path));
    }

    Glib::RefPtr<Gtk::FileDialog> createDialog(const FileDialogOptions& options)
    {
        const auto dialog = Gtk::FileDialog::create();
        dialog->set_title(options.title);
        dialog->set_modal(true);
        if (!options.initialName.empty())
            dialog->set_initial_name(options.initialName);
        return dialog;
    }
}

void showSaveFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->save(parent, [dialog, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::save_finish, onChosen, onFailed);
    });
}

void showOpenFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->open(parent, [dialog, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::open_finish, onChosen, onFailed);
    });
}

void showSelectFolderDialog(Gtk::Window& parent,
                            const FileDialogOptions& options,
                            FileChosenHandler onChosen,
                            FileFailedHandler onFailed)
{
    const auto dialog = createDialog(options);
    dialog->select_folder(parent, [dialog, onChosen = std::move(onChosen), onFailed = std::move(onFailed)](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        handleResult(dialog, result, &Gtk::FileDialog::select_folder_finish, onChosen, onFailed);
    });
}
