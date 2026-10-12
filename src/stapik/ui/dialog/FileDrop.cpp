#include "FileDrop.hpp"

#include "stapik/storage/PathText.hpp"
#include "stapik/text/TextFolding.hpp"

#include <gdk/gdk.h>
#include <gtkmm/droptarget.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
    bool hasAcceptedExtension(const std::filesystem::path& path, const std::vector<std::string>& extensions)
    {
        if (extensions.empty())
            return true;

        const auto extension = stapik::text::foldCase(stapik::storage::pathText(path.extension()));
        return std::ranges::any_of(extensions, [&extension](const std::string& accepted)
        {
            return stapik::text::foldCase(accepted) == extension;
        });
    }

    std::vector<std::filesystem::path> pathsOf(const Glib::ValueBase& value)
    {
        auto* fileList = static_cast<GdkFileList*>(g_value_get_boxed(value.gobj()));
        if (fileList == nullptr)
            return {};

        std::vector<std::filesystem::path> paths;
        GSList* files = gdk_file_list_get_files(fileList);
        for (const GSList* node = files; node != nullptr; node = node->next)
        {
            const std::unique_ptr<char, decltype(&g_free)> path(g_file_get_path(G_FILE(node->data)), &g_free);
            if (path)
                paths.push_back(stapik::storage::pathFromText(path.get()));
        }

        g_slist_free(files);
        return paths;
    }
}

namespace stapik::ui
{
    std::vector<std::filesystem::path> filterDroppedFiles(std::vector<std::filesystem::path> paths, const FileDropOptions& options)
    {
        std::erase_if(paths, [&options](const std::filesystem::path& path)
        {
            return !hasAcceptedExtension(path, options.extensions);
        });

        if (!options.allowMultiple && paths.size() > 1)
            paths.resize(1);

        return paths;
    }

    void acceptFileDrop(Gtk::Widget& target, FilesDroppedHandler onDropped, FileDropOptions options)
    {
        const auto dropTarget = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY);
        dropTarget->signal_drop().connect(
            [onDropped = std::move(onDropped), options = std::move(options)](const Glib::ValueBase& value, double, double)
            {
                const auto paths = filterDroppedFiles(pathsOf(value), options);
                if (paths.empty())
                    return false;

                if (onDropped)
                    onDropped(paths);
                return true;
            },
            false);
        target.add_controller(dropTarget);
    }
}
