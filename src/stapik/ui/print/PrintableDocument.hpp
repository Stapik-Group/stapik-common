#pragma once

#include <cairomm/context.h>

#include <string>

namespace stapik::ui
{
    // In points (1/72 inch), the unit of Cairo's PDF and print surfaces.
    struct PageSize
    {
        double width = 0.0;
        double height = 0.0;

        bool operator==(const PageSize&) const = default;
    };

    inline constexpr PageSize A4_PORTRAIT{ .width = 595.0, .height = 842.0 };

    class IPrintableDocument
    {
    public:
        virtual ~IPrintableDocument() = default;

        [[nodiscard]] virtual std::string title() const = 0;
        [[nodiscard]] virtual int pageCount(const PageSize& pageSize) const = 0;
        virtual void drawPage(const Cairo::RefPtr<Cairo::Context>& context, int pageIndex, const PageSize& pageSize) const = 0;
    };
}
