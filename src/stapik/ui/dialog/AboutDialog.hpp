#pragma once

#include "stapik/app/AppInfo.hpp"

#include <gtkmm/window.h>

void showAboutDialog(Gtk::Window& parent, const stapik::app::AppInfo& appInfo);
void showAboutDialog(Gtk::Window& parent);
