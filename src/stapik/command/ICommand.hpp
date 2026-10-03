#pragma once

#include <string>

namespace stapik::command
{
    class ICommand
    {
    public:
        virtual ~ICommand() = default;

        virtual void execute() = 0;
        virtual void undo() = 0;

        [[nodiscard]] virtual std::string description() const
        {
            return {};
        }

        [[nodiscard]] virtual bool mergeWith(const ICommand& next)
        {
            static_cast<void>(next);
            return false;
        }
    };
}
