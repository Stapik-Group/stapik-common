# Printing and PDF export

A printout is described once, as an `IPrintableDocument` (`stapik/ui/print/PrintableDocument.hpp`), and drawn with
Cairo. The same document goes to the print dialog and to a PDF file.

```cpp
class ReportDocument : public stapik::ui::IPrintableDocument
{
public:
    std::string title() const override { return "Monthly report"; }

    int pageCount(const stapik::ui::PageSize& pageSize) const override;
    void drawPage(const Cairo::RefPtr<Cairo::Context>& context, int pageIndex, const stapik::ui::PageSize& pageSize) const override;
};
```

`PageSize` is in points (1/72 inch). `pageCount` receives the size of the page that will be drawn: the printable area
of the chosen paper when printing, the size you pass for a PDF. The Cairo state is saved before and restored after
`drawPage`, so a page can change the transformation or the font freely.

## Printing

```cpp
stapik::ui::printDocument(window, std::make_shared<ReportDocument>(lines),
    [this](const std::string& error) { showError(error); });
```

`printDocument` opens the system print dialog (`Gtk::PrintOperation`) and keeps the document alive until the job is
over. Cancelling the dialog is not a failure. A document with no pages, an exception thrown by `drawPage` and an error
of the print system are passed to the failure handler once.

## PDF

```cpp
if (const auto exported = stapik::ui::exportToPdf(document, path); !exported)
    log::warning("Export failed: {}", exported.error().message());
```

`exportToPdf` renders the document in memory and writes it atomically (see `writeFileAtomically`), so a failed export
never leaves a half-written file. The page size defaults to `A4_PORTRAIT`. `renderPdf` returns the bytes of the PDF
instead, or the reason why it could not be made.
