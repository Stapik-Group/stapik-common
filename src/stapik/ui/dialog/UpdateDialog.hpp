#pragma once

#include "stapik/app/AppInfo.hpp"
#include "stapik/update/GitHubRelease.hpp"

#include <gtkmm/window.h>

void showUpdateAvailableDialog(Gtk::Window& parent, const stapik::app::AppInfo& appInfo, const stapik::update::ReleaseInfo& release);
