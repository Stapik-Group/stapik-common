#pragma once

#include "PrintableDocument.hpp"

#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <string>

namespace stapik::ui
{
    using PrintFailedHandler = std::function<void(const std::string&)>;

    // Opens the print dialog. The document is kept alive until printing is over; cancelling the dialog is not a failure.
    void printDocument(Gtk::Window& parent, std::shared_ptr<const IPrintableDocument> document, PrintFailedHandler onFailed = {});
}
