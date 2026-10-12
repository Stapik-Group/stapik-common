#include "stapik/ui/print/PdfExport.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;
    using stapik::ui::A4_PORTRAIT;
    using stapik::ui::exportToPdf;
    using stapik::ui::IPrintableDocument;
    using stapik::ui::PageSize;
    using stapik::ui::renderPdf;

    class SquaresDocument : public IPrintableDocument
    {
    public:
        explicit SquaresDocument(const int pages, const bool failOnDraw = false) :
            m_pages(pages),
            m_failOnDraw(failOnDraw)
        {}

        [[nodiscard]] std::string title() const override
        {
            return "Squares";
        }

        [[nodiscard]] int pageCount(const PageSize&) const override
        {
            return m_pages;
        }

        void drawPage(const Cairo::RefPtr<Cairo::Context>& context, const int, const PageSize& pageSize) const override
        {
            if (m_failOnDraw)
                throw std::runtime_error("cannot draw");

            context->rectangle(10.0, 10.0, pageSize.width / 2.0, pageSize.height / 2.0);
            context->fill();
        }

    private:
        int m_pages;
        bool m_failOnDraw;
    };

    class PdfExportTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-pdfexport-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(PdfExportTest, RenderedDocumentIsACompletePdf)
{
    const auto rendered = renderPdf(SquaresDocument(3));

    ASSERT_TRUE(rendered);
    EXPECT_TRUE(rendered.value().starts_with("%PDF-"));
    EXPECT_NE(rendered.value().rfind("%%EOF"), std::string::npos);
}

TEST_F(PdfExportTest, DocumentWithoutPagesIsRejected)
{
    const auto rendered = renderPdf(SquaresDocument(0));

    ASSERT_FALSE(rendered);
    EXPECT_FALSE(rendered.error().empty());
}

TEST_F(PdfExportTest, DrawingFailureIsReported)
{
    const auto rendered = renderPdf(SquaresDocument(1, true));

    ASSERT_FALSE(rendered);
    EXPECT_EQ(rendered.error(), "cannot draw");
}

TEST_F(PdfExportTest, ExportWritesTheFile)
{
    const auto path = m_directory / "squares.pdf";

    ASSERT_TRUE(exportToPdf(SquaresDocument(2), path, A4_PORTRAIT));

    std::ifstream stream(path, std::ios::binary);
    std::stringstream content;
    content << stream.rdbuf();
    EXPECT_TRUE(content.str().starts_with("%PDF-"));
}

TEST_F(PdfExportTest, FailedExportLeavesNoFile)
{
    const auto path = m_directory / "empty.pdf";

    const auto exported = exportToPdf(SquaresDocument(0), path);

    ASSERT_FALSE(exported);
    EXPECT_EQ(exported.error().path, path);
    EXPECT_FALSE(fs::exists(path));
}
