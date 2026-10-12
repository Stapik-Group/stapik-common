#include "PdfExport.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"

#include <cairomm/surface.h>

#include <exception>

namespace
{
    constexpr auto RENDER_STEP = "render PDF";

    void drawAllPages(const stapik::ui::IPrintableDocument& document, const stapik::ui::PageSize& pageSize, const int pageCount, const Cairo::RefPtr<Cairo::Context>& context)
    {
        for (int page = 0; page < pageCount; ++page)
        {
            context->save();
            document.drawPage(context, page, pageSize);
            context->restore();
            context->show_page();
        }
    }
}

namespace stapik::ui
{
    Result<std::string, std::string> renderPdf(const IPrintableDocument& document, const PageSize& pageSize)
    {
        const int pageCount = document.pageCount(pageSize);
        if (pageCount <= 0)
            return Failure<std::string>("the document has no pages");

        std::string content;
        try
        {
            const auto surface = Cairo::PdfSurface::create_for_stream(
                [&content](const unsigned char* data, const unsigned int length)
                {
                    content.append(reinterpret_cast<const char*>(data), length);
                    return CAIRO_STATUS_SUCCESS;
                },
                pageSize.width,
                pageSize.height);

            drawAllPages(document, pageSize, pageCount, Cairo::Context::create(surface));
            surface->finish();
        }
        catch (const std::exception& exception)
        {
            return Failure<std::string>(exception.what());
        }

        return content;
    }

    storage::FileResult exportToPdf(const IPrintableDocument& document, const std::filesystem::path& path, const PageSize& pageSize)
    {
        const auto rendered = renderPdf(document, pageSize);
        if (!rendered)
        {
            log::warning("Cannot render PDF '{}': {}", storage::pathText(path), rendered.error());
            return Failure(storage::FileError{ .path = path, .step = RENDER_STEP, .code = std::make_error_code(std::errc::io_error) });
        }

        return storage::writeFileAtomically(path, rendered.value());
    }
}
