#pragma once

#include <gtkmm/widget.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace stapik::ui
{
    struct FileDropOptions
    {
        // With a leading dot, compared without regard to case, for example ".csv". Empty accepts every file.
        std::vector<std::string> extensions;
        bool allowMultiple = true;
    };

    using FilesDroppedHandler = std::function<void(const std::vector<std::filesystem::path>&)>;

    [[nodiscard]] std::vector<std::filesystem::path> filterDroppedFiles(std::vector<std::filesystem::path> paths, const FileDropOptions& options);

    // A drop with no acceptable file is rejected, so the drag source shows that nothing was taken.
    void acceptFileDrop(Gtk::Widget& target, FilesDroppedHandler onDropped, FileDropOptions options = {});
}
