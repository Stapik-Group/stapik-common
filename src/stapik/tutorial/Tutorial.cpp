#include "Tutorial.hpp"

#include <algorithm>
#include <iterator>

namespace stapik::tutorial
{
    namespace
    {
        void appendInReadingOrder(const std::vector<Chapter>& chapters, std::vector<const Chapter*>& order)
        {
            for (const auto& chapter : chapters)
            {
                order.push_back(&chapter);
                appendInReadingOrder(chapter.children, order);
            }
        }
    }

    bool Tutorial::empty() const
    {
        return chapters.empty();
    }

    std::vector<const Chapter*> Tutorial::readingOrder() const
    {
        std::vector<const Chapter*> order;
        appendInReadingOrder(chapters, order);
        return order;
    }

    const Chapter* Tutorial::find(const std::string_view id) const
    {
        const auto order = readingOrder();
        const auto match = std::ranges::find(order, id, &Chapter::id);
        return match == order.end() ? nullptr : *match;
    }

    const Chapter* Tutorial::first() const
    {
        return chapters.empty() ? nullptr : &chapters.front();
    }

    const Chapter* Tutorial::previous(const std::string_view id) const
    {
        const auto order = readingOrder();
        const auto match = std::ranges::find(order, id, &Chapter::id);
        return match == order.end() || match == order.begin() ? nullptr : *std::prev(match);
    }

    const Chapter* Tutorial::next(const std::string_view id) const
    {
        const auto order = readingOrder();
        const auto match = std::ranges::find(order, id, &Chapter::id);
        return match == order.end() || std::next(match) == order.end() ? nullptr : *std::next(match);
    }
}
