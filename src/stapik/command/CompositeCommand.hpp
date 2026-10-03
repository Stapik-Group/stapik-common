#pragma once

#include "ICommand.hpp"

#include <memory>
#include <string>
#include <vector>

namespace stapik::command
{
    class CompositeCommand final : public ICommand
    {
    public:
        explicit CompositeCommand(std::string description = {});

        CompositeCommand& add(std::unique_ptr<ICommand> command);

        void execute() override;
        void undo() override;

        [[nodiscard]] std::string description() const override;
        [[nodiscard]] std::size_t size() const;

    private:
        std::string m_description;
        std::vector<std::unique_ptr<ICommand>> m_commands;
        std::size_t m_executed = 0;
    };
}
