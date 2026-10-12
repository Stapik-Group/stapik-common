#include "TutorialLoader.hpp"

#include "stapik/core/Result.hpp"
#include "stapik/locale/LanguageCode.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/markdown/MarkdownParser.hpp"
#include "stapik/storage/FileError.hpp"
#include "stapik/storage/PathText.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace stapik::tutorial
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::string_view UTF8_BOM = "\xEF\xBB\xBF";
        constexpr std::string_view ORDER_SEPARATORS = "-_.";
        constexpr char ID_SEPARATOR = '/';

        struct ChapterSource
        {
            fs::path path;
            std::string name;
            bool isBook = false;
        };

        Result<std::string, storage::FileError> readTextFile(const fs::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return Failure(storage::FileError{ .path = path, .step = "open", .code = std::error_code(errno, std::generic_category()) });

            std::string text((std::istreambuf_iterator(stream)), std::istreambuf_iterator<char>());
            if (text.starts_with(UTF8_BOM))
                text.erase(0, UTF8_BOM.size());
            return text;
        }

        std::string withoutOrderPrefix(const std::string& name)
        {
            const auto digitsEnd = name.find_first_not_of("0123456789");
            if (digitsEnd == 0 || digitsEnd == std::string::npos || ORDER_SEPARATORS.find(name[digitsEnd]) == std::string_view::npos)
                return name;

            const auto nameStart = name.find_first_not_of(' ', digitsEnd + 1);
            return nameStart == std::string::npos ? name : name.substr(nameStart);
        }

        std::string titleFromName(const std::string& name)
        {
            std::string title = name;
            std::ranges::replace_if(title, [](const char character) { return character == '-' || character == '_'; }, ' ');
            if (!title.empty())
                title.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(title.front())));
            return title;
        }

        std::string titleOf(const markdown::Document& content, const std::string& name)
        {
            if (const auto heading = std::ranges::find(content, markdown::BlockKind::Heading, &markdown::Block::kind); heading != content.end())
            {
                if (auto title = markdown::plainText(heading->spans); !title.empty())
                    return title;
            }
            return titleFromName(name);
        }

        std::string joinId(const std::string& parentId, const std::string& name)
        {
            return parentId.empty() ? name : parentId + ID_SEPARATOR + name;
        }

        std::string nameOf(const fs::path& path, const bool isBook)
        {
            return storage::pathText(isBook ? path.filename() : path.stem());
        }

        bool isChapterFile(const fs::directory_entry& entry, std::error_code& errorCode)
        {
            return entry.is_regular_file(errorCode) && entry.path().extension() == CHAPTER_EXTENSION && entry.path().filename() != BOOK_PAGE_FILE;
        }

        bool isDirectory(const fs::path& path)
        {
            std::error_code errorCode;
            return fs::is_directory(path, errorCode);
        }

        bool isRegularFile(const fs::path& path)
        {
            std::error_code errorCode;
            return fs::is_regular_file(path, errorCode);
        }

        std::vector<ChapterSource> listSources(const fs::path& directory)
        {
            std::vector<ChapterSource> sources;
            std::error_code errorCode;

            for (fs::directory_iterator it(directory, errorCode); !errorCode && it != fs::directory_iterator(); it.increment(errorCode))
            {
                const auto& entry = *it;
                std::error_code entryError;
                const bool isBook = entry.is_directory(entryError);
                if (!isBook && !isChapterFile(entry, entryError))
                    continue;

                if (auto name = nameOf(entry.path(), isBook); !name.starts_with('.'))
                    sources.push_back({ .path = entry.path(), .name = std::move(name), .isBook = isBook });
            }

            std::ranges::sort(sources, {}, &ChapterSource::name);
            return sources;
        }

        markdown::Document readContent(const fs::path& file)
        {
            auto text = readTextFile(file);
            if (!text)
            {
                log::warning("Cannot read tutorial page: {}", text.error().message());
                return {};
            }
            return markdown::parseMarkdown(*text);
        }

        markdown::Document contentsPage(const std::string& title, const std::vector<Chapter>& children)
        {
            markdown::Document page;

            markdown::Block heading;
            heading.kind = markdown::BlockKind::Heading;
            heading.headingLevel = 1;
            heading.spans = markdown::parseInline(title);
            page.push_back(std::move(heading));

            for (const auto& child : children)
            {
                markdown::Span link;
                link.text = child.title;
                link.link = "#" + child.id;

                markdown::Block item;
                item.kind = markdown::BlockKind::ListItem;
                item.spans.push_back(std::move(link));
                page.push_back(std::move(item));
            }
            return page;
        }

        std::vector<Chapter> loadChapters(const fs::path& directory, const std::string& parentId);

        std::optional<Chapter> loadBook(const ChapterSource& source, const std::string& parentId)
        {
            Chapter book;
            const auto name = withoutOrderPrefix(source.name);
            book.id = joinId(parentId, name);
            book.directory = source.path;
            book.children = loadChapters(source.path, book.id);

            if (const auto pageFile = source.path / BOOK_PAGE_FILE; isRegularFile(pageFile))
                book.content = readContent(pageFile);

            if (book.content.empty() && book.children.empty())
                return std::nullopt;

            book.title = titleOf(book.content, name);
            if (book.content.empty())
                book.content = contentsPage(book.title, book.children);
            return book;
        }

        std::optional<Chapter> loadPage(const ChapterSource& source, const std::string& parentId)
        {
            Chapter page;
            const auto name = withoutOrderPrefix(source.name);
            page.id = joinId(parentId, name);
            page.directory = source.path.parent_path();
            page.content = readContent(source.path);
            if (page.content.empty())
                return std::nullopt;

            page.title = titleOf(page.content, name);
            return page;
        }

        std::vector<Chapter> loadChapters(const fs::path& directory, const std::string& parentId)
        {
            std::vector<Chapter> chapters;
            for (const auto& source : listSources(directory))
            {
                if (auto chapter = source.isBook ? loadBook(source, parentId) : loadPage(source, parentId))
                    chapters.push_back(std::move(*chapter));
            }
            return chapters;
        }
    }

    std::optional<fs::path> findLanguageDirectory(const fs::path& tutorialDirectory, const std::string_view languageCode)
    {
        for (const auto& language : { locale::primaryLanguage(languageCode), std::string(FALLBACK_LANGUAGE) })
        {
            if (const auto directory = tutorialDirectory / language; !language.empty() && isDirectory(directory))
                return directory;
        }
        return std::nullopt;
    }

    Tutorial loadTutorial(const fs::path& tutorialDirectory, const std::string_view languageCode)
    {
        const auto directory = findLanguageDirectory(tutorialDirectory, languageCode);
        if (!directory)
        {
            log::info("No tutorial for '{}' in {}", languageCode, storage::pathText(tutorialDirectory));
            return {};
        }

        return Tutorial{ .chapters = loadChapters(*directory, {}) };
    }
}
