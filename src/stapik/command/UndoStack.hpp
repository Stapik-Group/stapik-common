#pragma once

#include "ICommand.hpp"

#include <sigc++/signal.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <string>

namespace stapik::command
{
    class UndoStack
    {
    public:
        explicit UndoStack(std::size_t maxDepth = 100);

        UndoStack(const UndoStack&) = delete;
        UndoStack& operator=(const UndoStack&) = delete;

        void execute(std::unique_ptr<ICommand> command);
        bool undo();
        bool redo();
        void clear();
        void breakMergeChain();
        void setMaxDepth(std::size_t maxDepth);

        [[nodiscard]] bool canUndo() const;
        [[nodiscard]] bool canRedo() const;
        [[nodiscard]] std::string undoDescription() const;
        [[nodiscard]] std::string redoDescription() const;
        [[nodiscard]] std::size_t undoDepth() const;
        [[nodiscard]] std::size_t redoDepth() const;

        sigc::signal<void()>& signalChanged();

    private:
        void trimToMaxDepth();

        std::size_t m_maxDepth;
        std::deque<std::unique_ptr<ICommand>> m_undoCommands;
        std::deque<std::unique_ptr<ICommand>> m_redoCommands;
        bool m_canMerge = false;
        sigc::signal<void()> m_signalChanged;
    };
}
