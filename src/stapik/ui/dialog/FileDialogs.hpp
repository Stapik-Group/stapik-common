#pragma once

#include <glibmm/ustring.h>
#include <gtkmm/window.h>

#include <filesystem>
#include <functional>
#include <string>

struct FileDialogOptions
{
    Glib::ustring title;
    std::string initialName;
};

using FileChosenHandler = std::function<void(const std::filesystem::path&)>;
using FileFailedHandler = std::function<void(const std::string&)>;

void showSaveFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed = {});

void showOpenFileDialog(Gtk::Window& parent,
                        const FileDialogOptions& options,
                        FileChosenHandler onChosen,
                        FileFailedHandler onFailed = {});

void showSelectFolderDialog(Gtk::Window& parent,
                            const FileDialogOptions& options,
                            FileChosenHandler onChosen,
                            FileFailedHandler onFailed = {});
