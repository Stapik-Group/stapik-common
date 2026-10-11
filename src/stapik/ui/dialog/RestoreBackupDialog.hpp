#pragma once

#include "stapik/document/BackupInfo.hpp"

#include <gtkmm/window.h>

#include <functional>
#include <vector>

void showRestoreBackupDialog(
    Gtk::Window& parent,
    std::vector<stapik::document::BackupInfo> backups,
    std::function<void(const stapik::document::BackupInfo&)> onRestore);
