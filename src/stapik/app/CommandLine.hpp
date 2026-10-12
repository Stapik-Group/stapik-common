#pragma once

#include "AppInfo.hpp"

#include "stapik/log/Log.hpp"

#include <giomm/application.h>
#include <glibmm/variantdict.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace stapik::app
{
    struct CommandLineOptions
    {
        bool showVersion = false;
        std::optional<log::Level> logLevel;
        std::optional<std::filesystem::path> dataDir;
        bool portable = false;
        bool resetSettings = false;
        bool safeMode = false;
        std::vector<std::filesystem::path> files;
    };

    struct CommandLineParseResult
    {
        CommandLineOptions options;
        std::string error;

        [[nodiscard]] bool ok() const
        {
            return error.empty();
        }
    };

    [[nodiscard]] CommandLineParseResult readCommandLineOptions(const Glib::VariantDict &values);
    [[nodiscard]] std::string versionLine(const AppInfo &appInfo);

    void addCommandLineOptions(Gio::Application &application);
    // Applies what has to be in place before settings and user data are touched. Call it once, early.
    void applyCommandLineOptions(const CommandLineOptions &options, const std::string &appName);
}
