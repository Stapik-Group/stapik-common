#include "UndoStack.hpp"

#include <algorithm>
#include <utility>

namespace stapik::command
{
    UndoStack::UndoStack(const std::size_t maxDepth) :
        m_maxDepth(std::max<std::size_t>(maxDepth, 1))
    {}

    void UndoStack::execute(std::unique_ptr<ICommand> command)
    {
        command->execute();

        m_redoCommands.clear();

        if (const bool merged = m_canMerge && !m_undoCommands.empty() && m_undoCommands.back()->mergeWith(*command); !merged)
            m_undoCommands.push_back(std::move(command));

        m_canMerge = true;
        trimToMaxDepth();
        m_signalChanged.emit();
    }

    bool UndoStack::undo()
    {
        if (m_undoCommands.empty())
            return false;

        m_undoCommands.back()->undo();

        m_redoCommands.push_back(std::move(m_undoCommands.back()));
        m_undoCommands.pop_back();
        m_canMerge = false;
        m_signalChanged.emit();
        return true;
    }

    bool UndoStack::redo()
    {
        if (m_redoCommands.empty())
            return false;

        m_redoCommands.back()->execute();

        m_undoCommands.push_back(std::move(m_redoCommands.back()));
        m_redoCommands.pop_back();
        m_canMerge = false;
        m_signalChanged.emit();
        return true;
    }

    void UndoStack::clear()
    {
        if (m_undoCommands.empty() && m_redoCommands.empty())
            return;

        m_undoCommands.clear();
        m_redoCommands.clear();
        m_canMerge = false;
        m_signalChanged.emit();
    }

    void UndoStack::breakMergeChain()
    {
        m_canMerge = false;
    }

    void UndoStack::setMaxDepth(const std::size_t maxDepth)
    {
        m_maxDepth = std::max<std::size_t>(maxDepth, 1);

        const auto before = m_undoCommands.size();
        trimToMaxDepth();

        if (m_undoCommands.size() != before)
            m_signalChanged.emit();
    }

    bool UndoStack::canUndo() const
    {
        return !m_undoCommands.empty();
    }

    bool UndoStack::canRedo() const
    {
        return !m_redoCommands.empty();
    }

    std::string UndoStack::undoDescription() const
    {
        return m_undoCommands.empty() ? std::string() : m_undoCommands.back()->description();
    }

    std::string UndoStack::redoDescription() const
    {
        return m_redoCommands.empty() ? std::string() : m_redoCommands.back()->description();
    }

    std::size_t UndoStack::undoDepth() const
    {
        return m_undoCommands.size();
    }

    std::size_t UndoStack::redoDepth() const
    {
        return m_redoCommands.size();
    }

    sigc::signal<void()>& UndoStack::signalChanged()
    {
        return m_signalChanged;
    }

    void UndoStack::trimToMaxDepth()
    {
        while (m_undoCommands.size() > m_maxDepth)
            m_undoCommands.pop_front();
    }
}
