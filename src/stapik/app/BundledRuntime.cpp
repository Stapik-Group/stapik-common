#include "BundledRuntime.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/log/FileSink.hpp"
#include "stapik/log/GLibLogBridge.hpp"
#include "stapik/log/MemoryLog.hpp"
#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace
{
    constexpr std::string_view PIXBUF_LOADERS_CACHE_NAME = "pixbuf-loaders.cache";
    constexpr auto LOG_FILE_NAME = "stapik.log";

    bool endsWithCaseInsensitive(const std::string_view text, const std::string_view suffix)
    {
        return text.size() >= suffix.size()
            && std::equal(suffix.begin(), suffix.end(), text.end() - static_cast<std::ptrdiff_t>(suffix.size()), [](const char left, const char right)
            {
                return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
            });
    }

    // "C:\\\\msys64\\\\lib\\\\x.dll" as written in the cache -> C:\msys64\lib\x.dll
    std::string unescapeCacheString(const std::string_view text)
    {
        std::string result;
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '\\' && index + 1 < text.size() && (text[index + 1] == '\\' || text[index + 1] == '"'))
                ++index;

            result += text[index];
        }

        return result;
    }

    std::string escapeCacheString(const std::string_view text)
    {
        std::string result;
        for (const char character : text)
        {
            if (character == '\\' || character == '"')
                result += '\\';

            result += character;
        }

        return result;
    }

    bool isLoaderModuleLine(const std::string_view content)
    {
        // The module line holds a single quoted path; the description lines hold several quoted strings.
        return content.find('"') == std::string_view::npos
            && (endsWithCaseInsensitive(content, ".dll") || endsWithCaseInsensitive(content, ".so") || endsWithCaseInsensitive(content, ".dylib"));
    }

    std::string rebaseLine(const std::string_view line, const std::filesystem::path& loadersDirectory)
    {
        std::string_view body = line;
        std::string_view lineEnd;
        if (body.ends_with('\r'))
        {
            lineEnd = body.substr(body.size() - 1);
            body.remove_suffix(1);
        }

        if (body.size() < 2 || body.front() != '"' || body.back() != '"')
            return std::string(line);

        const auto content = body.substr(1, body.size() - 2);
        if (!isLoaderModuleLine(content))
            return std::string(line);

        const auto path = unescapeCacheString(content);
        const auto fileName = path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
        const auto rebased = stapik::storage::pathText(loadersDirectory / std::filesystem::path(std::u8string(fileName.begin(), fileName.end())));

        return "\"" + escapeCacheString(rebased) + "\"" + std::string(lineEnd);
    }

    std::optional<std::string> readTextFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            return std::nullopt;

        return std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
    }

    bool isDirectory(const std::filesystem::path& directory)
    {
        std::error_code errorCode;
        return std::filesystem::is_directory(directory, errorCode);
    }

    void setEnvironment(const char* name, const std::filesystem::path& value)
    {
        const auto text = stapik::storage::pathText(value);
        stapik::log::debug("{}={}", name, text);
        g_setenv(name, text.c_str(), TRUE);
    }

    void prependSearchPath(const char* name, const std::filesystem::path& directory)
    {
        const auto ours = stapik::storage::pathText(directory);

        const auto* existing = g_getenv(name);
        if (existing == nullptr || *existing == '\0')
        {
            g_setenv(name, ours.c_str(), TRUE);
            return;
        }

        const std::string_view current = existing;
        if (current == ours || current.starts_with(ours + G_SEARCHPATH_SEPARATOR_S))
            return;

        g_setenv(name, (ours + G_SEARCHPATH_SEPARATOR_S + std::string(current)).c_str(), TRUE);
    }

    std::optional<std::filesystem::path> findPixbufModuleDirectory(const std::filesystem::path& bundleRoot)
    {
        for (std::error_code errorCode; const auto& entry : std::filesystem::directory_iterator(bundleRoot / "lib" / "gdk-pixbuf-2.0", errorCode))
        {
            if (entry.is_directory(errorCode) && std::filesystem::is_regular_file(entry.path() / "loaders.cache", errorCode))
                return entry.path();
        }

        return std::nullopt;
    }

    void configurePixbufLoaders(const std::filesystem::path& bundleRoot, const std::filesystem::path& cacheDirectory)
    {
        const auto moduleDirectory = findPixbufModuleDirectory(bundleRoot);
        if (!moduleDirectory)
            return;

        const auto cacheText = readTextFile(*moduleDirectory / "loaders.cache");
        if (!cacheText)
        {
            stapik::log::warning("Cannot read {}", stapik::storage::pathText(*moduleDirectory / "loaders.cache"));
            return;
        }

        const auto rebasedCache = stapik::app::rebasePixbufLoadersCache(*cacheText, *moduleDirectory / "loaders");
        const auto cacheFile = cacheDirectory / std::string(PIXBUF_LOADERS_CACHE_NAME);
        if (!stapik::storage::writeFileAtomically(cacheFile, rebasedCache))
        {
            stapik::log::warning("Cannot write {}; image loaders may not be found", stapik::storage::pathText(cacheFile));
            return;
        }

        setEnvironment("GDK_PIXBUF_MODULE_FILE", cacheFile);
    }

    bool shouldWriteLogFile()
    {
        if (const auto* setting = g_getenv("STAPIK_LOG_FILE"); setting != nullptr && std::string_view(setting) == "off")
            return false;

#ifdef _WIN32
        return true;
#else
        // Started from a terminal, the output belongs there.
        return isatty(fileno(stderr)) == 0;
#endif
    }
}

namespace stapik::app
{
    std::string rebasePixbufLoadersCache(const std::string_view cacheText, const std::filesystem::path& loadersDirectory)
    {
        std::string result;
        result.reserve(cacheText.size());

        std::size_t position = 0;
        while (true)
        {
            const auto lineEnd = cacheText.find('\n', position);
            if (lineEnd == std::string_view::npos)
            {
                result += rebaseLine(cacheText.substr(position), loadersDirectory);
                break;
            }

            result += rebaseLine(cacheText.substr(position, lineEnd - position), loadersDirectory);
            result += '\n';
            position = lineEnd + 1;
        }

        return result;
    }

    void configureBundledRuntime(const std::filesystem::path& bundleRoot, const std::filesystem::path& cacheDirectory)
    {
        const auto share = bundleRoot / "share";

        if (const auto schemas = share / "glib-2.0" / "schemas"; isDirectory(schemas))
            setEnvironment("GSETTINGS_SCHEMA_DIR", schemas);

        if (isDirectory(share))
            prependSearchPath("XDG_DATA_DIRS", share);

        if (const auto fonts = bundleRoot / "etc" / "fonts"; isDirectory(fonts))
            setEnvironment("FONTCONFIG_PATH", fonts);

        configurePixbufLoaders(bundleRoot, cacheDirectory);
    }

    void prepareLogging(const std::string& appName)
    {
        static std::once_flag once;
        std::call_once(once, [&appName]
        {
            log::forwardGLibMessagesToLog();
            log::installMemoryLog();

            if (!shouldWriteLogFile())
                return;

            const auto cacheDirectory = AppPaths::userCacheDir(appName);
            std::error_code errorCode;
            std::filesystem::create_directories(cacheDirectory, errorCode);

            log::addSink(std::make_shared<log::FileSink>(cacheDirectory / LOG_FILE_NAME));
        });
    }

    void prepareWindowsRuntime(const std::string& appName)
    {
#ifdef _WIN32
        const auto cacheDirectory = AppPaths::userCacheDir(appName);
        std::error_code errorCode;
        std::filesystem::create_directories(cacheDirectory, errorCode);

        const auto bundleRoot = AppPaths::executableDir();
        configureBundledRuntime(bundleRoot, cacheDirectory);

        if (g_getenv("GSK_RENDERER") == nullptr)
            g_setenv("GSK_RENDERER", "cairo", FALSE);

        log::info("Runtime of {} prepared in {}", appName, storage::pathText(bundleRoot));
#else
        static_cast<void>(appName);
#endif
    }

    void preferStableRenderer()
    {
#ifdef __linux__
        if (g_getenv("GSK_RENDERER") == nullptr)
            g_setenv("GSK_RENDERER", "cairo", FALSE);
#endif
    }
}
