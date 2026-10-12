#pragma once

#include <gtkmm/window.h>

#include <filesystem>
#include <string>

void showTutorialWindow(Gtk::Window& parent, const std::filesystem::path& tutorialDirectory, const std::string& chapterId = {});
