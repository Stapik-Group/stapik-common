#include "CommandLine.hpp"

#include "stapik/settings/AppSettings.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"

#include <glibmm/variant.h>

#include <format>

namespace
{
    constexpr auto FILES_KEY = G_OPTION_REMAINING;

    std::optional<std::string> lookupString(const Glib::VariantDict &values, const char *key)
    {
        Glib::VariantBase value;
        if (!values.lookup_value(key, value))
            return std::nullopt;

        return Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring> >(value).get().raw();
    }

    std::filesystem::path absolutePath(const std::string &text)
    {
        const auto path = stapik::storage::pathFromText(text);

        std::error_code errorCode;
        const auto result = std::filesystem::absolute(path, errorCode);
        return errorCode ? path : result.lexically_normal();
    }

    std::vector<std::filesystem::path> lookupPaths(const Glib::VariantDict &values, const char *key)
    {
        Glib::VariantBase value;
        if (!values.lookup_value(key, value))
            return {};

        std::vector<std::filesystem::path> paths;
        for (const auto &text: Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::string> > >(value).get())
        {
            if (!text.empty())
                paths.push_back(absolutePath(text));
        }
        return paths;
    }
}

namespace stapik::app
{
    void addCommandLineOptions(Gio::Application &application)
    {
        using enum Gio::Application::OptionType;

        application.add_main_option_entry(BOOL, "version", '\0', "Print the version and exit");
        application.add_main_option_entry(STRING, "log-level", '\0',
                                          "Minimum log level: debug, info, warning or error", "LEVEL");
        application.add_main_option_entry(STRING, "data-dir", '\0',
                                          "Keep user data, settings and cache under DIR instead of the system locations",
                                          "DIR");
        application.add_main_option_entry(BOOL, "portable", '\0',
                                          "Keep user data, settings and cache in the 'portable' folder next to the executable");
        application.add_main_option_entry(BOOL, "reset-settings", '\0',
                                          "Reset settings to defaults before starting (the old file is kept as settings.json.bak)");
        application.add_main_option_entry(BOOL, "safe-mode", '\0',
                                          "Start without restoring or saving the window state, for troubleshooting");
        application.add_main_option_entry(FILENAME_VECTOR, FILES_KEY, '\0', "Files to open", "[FILE...]");
    }

    CommandLineParseResult readCommandLineOptions(const Glib::VariantDict &values)
    {
        CommandLineParseResult result;
        auto &[showVersion, logLevel, dataDir, portable, resetSettings, safeMode, files] = result.options;

        showVersion = values.contains("version");
        portable = values.contains("portable");
        resetSettings = values.contains("reset-settings");
        safeMode = values.contains("safe-mode");
        files = lookupPaths(values, FILES_KEY);

        if (const auto levelName = lookupString(values, "log-level"))
        {
            logLevel = log::parseLevel(*levelName);
            if (!logLevel)
            {
                result.error = std::format("invalid --log-level '{}' (expected debug, info, warning or error)",
                                           *levelName);
                return result;
            }
        }

        if (const auto dataDirFromVariantDictValues = lookupString(values, "data-dir"))
        {
            if (dataDirFromVariantDictValues->empty())
            {
                result.error = "--data-dir requires a non-empty path";
                return result;
            }

            if (portable)
            {
                result.error = "--data-dir and --portable cannot be used together";
                return result;
            }

            dataDir = absolutePath(*dataDirFromVariantDictValues);
        }

        return result;
    }

    std::string versionLine(const AppInfo &appInfo)
    {
        const auto &name = appInfo.displayName.empty() ? appInfo.internalName : appInfo.displayName;
        return std::format("{} {}", name, appInfo.version.empty() ? "(unknown version)" : appInfo.version);
    }

    void applyCommandLineOptions(const CommandLineOptions &options, const std::string &appName)
    {
        if (options.logLevel)
            log::setLevel(*options.logLevel);

        if (options.portable)
            AppPaths::setRootOverride(AppPaths::portableRoot());
        else if (options.dataDir)
            AppPaths::setRootOverride(*options.dataDir);

        if (const auto root = AppPaths::rootOverride())
            log::info("User data is kept in {}", storage::pathText(*root));

        if (options.resetSettings && !settings::resetAppSettings(appName))
            log::warning("Settings could not be fully reset");

        if (options.safeMode)
            log::info("Safe mode: the window state is neither restored nor saved");
    }
}
