#include "CompositeCommand.hpp"

#include <utility>

namespace stapik::command
{
    CompositeCommand::CompositeCommand(std::string description) :
        m_description(std::move(description))
    {}

    CompositeCommand& CompositeCommand::add(std::unique_ptr<ICommand> command)
    {
        m_commands.push_back(std::move(command));
        return *this;
    }

    void CompositeCommand::execute()
    {
        m_executed = 0;

        try
        {
            for (auto const& command : m_commands)
            {
                command->execute();
                ++m_executed;
            }
        }
        catch (...)
        {
            undo();
            throw;
        }
    }

    void CompositeCommand::undo()
    {
        while (m_executed > 0)
        {
            --m_executed;
            m_commands[m_executed]->undo();
        }
    }

    std::string CompositeCommand::description() const
    {
        return m_description;
    }

    std::size_t CompositeCommand::size() const
    {
        return m_commands.size();
    }
}
