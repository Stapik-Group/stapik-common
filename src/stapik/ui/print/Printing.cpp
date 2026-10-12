#include "Printing.hpp"

#include <glibmm/error.h>
#include <gtk/gtk.h>
#include <gtkmm/printcontext.h>
#include <gtkmm/printoperation.h>

#include <algorithm>
#include <exception>
#include <utility>
#include <vector>

namespace
{
    using stapik::ui::PageSize;

    struct PrintJob
    {
        Glib::RefPtr<Gtk::PrintOperation> operation;
        std::shared_ptr<const stapik::ui::IPrintableDocument> document;
        stapik::ui::PrintFailedHandler onFailed;
        bool failureReported = false;
    };

    std::vector<std::unique_ptr<PrintJob>> runningJobs;

    PageSize pageSizeOf(const Glib::RefPtr<Gtk::PrintContext>& context)
    {
        return { .width = context->get_width(), .height = context->get_height() };
    }

    void reportFailure(PrintJob& job, const std::string& message)
    {
        if (job.failureReported)
            return;

        job.failureReported = true;
        if (job.onFailed)
            job.onFailed(message);
    }

    void abort(PrintJob& job, const std::string& message)
    {
        job.operation->cancel();
        reportFailure(job, message);
    }

    std::string operationError(const Glib::RefPtr<Gtk::PrintOperation>& operation)
    {
        GError* error = nullptr;
        gtk_print_operation_get_error(operation->gobj(), &error);
        if (error == nullptr)
            return {};

        const std::string message = error->message;
        g_error_free(error);
        return message;
    }

    void beginPrint(PrintJob& job, const Glib::RefPtr<Gtk::PrintContext>& context)
    {
        const int pageCount = job.document->pageCount(pageSizeOf(context));
        if (pageCount <= 0)
        {
            abort(job, "the document has no pages");
            return;
        }

        job.operation->set_n_pages(pageCount);
    }

    void drawPage(PrintJob& job, const Glib::RefPtr<Gtk::PrintContext>& context, const int pageIndex)
    {
        try
        {
            const auto cairoContext = context->get_cairo_context();
            cairoContext->save();
            job.document->drawPage(cairoContext, pageIndex, pageSizeOf(context));
            cairoContext->restore();
        }
        catch (const std::exception& exception)
        {
            abort(job, exception.what());
        }
    }

    void finish(PrintJob& job, const Gtk::PrintOperation::Result result)
    {
        if (result == Gtk::PrintOperation::Result::ERROR)
            reportFailure(job, operationError(job.operation));

        std::erase_if(runningJobs, [&job](const std::unique_ptr<PrintJob>& running) { return running.get() == &job; });
    }
}

namespace stapik::ui
{
    void printDocument(Gtk::Window& parent, std::shared_ptr<const IPrintableDocument> document, PrintFailedHandler onFailed)
    {
        auto& job = *runningJobs.emplace_back(std::make_unique<PrintJob>(Gtk::PrintOperation::create(), std::move(document), std::move(onFailed)));

        job.operation->set_job_name(job.document->title());
        job.operation->set_unit(Gtk::Unit::POINTS);
        job.operation->signal_begin_print().connect([&job](const Glib::RefPtr<Gtk::PrintContext>& context) { beginPrint(job, context); });
        job.operation->signal_draw_page().connect([&job](const Glib::RefPtr<Gtk::PrintContext>& context, const int pageIndex) { drawPage(job, context, pageIndex); });
        job.operation->signal_done().connect([&job](const Gtk::PrintOperation::Result result) { finish(job, result); });

        try
        {
            job.operation->run(Gtk::PrintOperation::Action::PRINT_DIALOG, parent);
        }
        catch (const Glib::Error& error)
        {
            reportFailure(job, error.what());
            finish(job, Gtk::PrintOperation::Result::CANCEL);
        }
    }
}
