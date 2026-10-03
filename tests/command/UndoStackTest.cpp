#include "stapik/command/CompositeCommand.hpp"
#include "stapik/command/UndoStack.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using stapik::command::CompositeCommand;
    using stapik::command::ICommand;
    using stapik::command::UndoStack;

    class RecordingCommand final : public ICommand
    {
    public:
        RecordingCommand(std::vector<std::string>& log, std::string name, std::string description = {}) :
            m_log(log),
            m_name(std::move(name)),
            m_description(std::move(description))
        {}

        void execute() override
        {
            if (failOnExecute)
                throw std::runtime_error("execute failed");

            m_log.push_back("do " + m_name);
        }

        void undo() override
        {
            if (failOnUndo)
                throw std::runtime_error("undo failed");

            m_log.push_back("undo " + m_name);
        }

        [[nodiscard]] std::string description() const override
        {
            return m_description;
        }

        bool failOnExecute = false;
        bool failOnUndo = false;

    private:
        std::vector<std::string>& m_log;
        std::string m_name;
        std::string m_description;
    };

    class SetValueCommand final : public ICommand
    {
    public:
        SetValueCommand(int& target, const int newValue) :
            m_target(target),
            m_oldValue(target),
            m_newValue(newValue)
        {}

        void execute() override
        {
            m_target = m_newValue;
        }

        void undo() override
        {
            m_target = m_oldValue;
        }

        [[nodiscard]] bool mergeWith(const ICommand& next) override
        {
            const auto* other = dynamic_cast<const SetValueCommand*>(&next);
            if (other == nullptr || &other->m_target != &m_target)
                return false;

            m_newValue = other->m_newValue;
            return true;
        }

    private:
        int& m_target;
        int m_oldValue;
        int m_newValue;
    };

    std::unique_ptr<ICommand> record(std::vector<std::string>& log, const std::string& name, const std::string& description = {})
    {
        return std::make_unique<RecordingCommand>(log, name, description);
    }
}

TEST(UndoStackTest, ExecuteRunsTheCommandAndEnablesUndo)
{
    std::vector<std::string> log;
    UndoStack stack;

    stack.execute(record(log, "a"));

    EXPECT_EQ(log, (std::vector<std::string>{ "do a" }));
    EXPECT_TRUE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());
}

TEST(UndoStackTest, UndoAndRedoWalkThroughTheHistory)
{
    std::vector<std::string> log;
    UndoStack stack;
    stack.execute(record(log, "a"));
    stack.execute(record(log, "b"));
    log.clear();

    EXPECT_TRUE(stack.undo());
    EXPECT_TRUE(stack.undo());
    EXPECT_FALSE(stack.undo());
    EXPECT_TRUE(stack.redo());
    EXPECT_TRUE(stack.redo());
    EXPECT_FALSE(stack.redo());

    EXPECT_EQ(log, (std::vector<std::string>{ "undo b", "undo a", "do a", "do b" }));
}

TEST(UndoStackTest, ExecutingAfterUndoClearsTheRedoHistory)
{
    std::vector<std::string> log;
    UndoStack stack;
    stack.execute(record(log, "a"));
    stack.undo();
    ASSERT_TRUE(stack.canRedo());

    stack.execute(record(log, "b"));

    EXPECT_FALSE(stack.canRedo());
    EXPECT_EQ(stack.undoDepth(), 1u);
}

TEST(UndoStackTest, DepthLimitDropsTheOldestCommands)
{
    std::vector<std::string> log;
    UndoStack stack(3);

    for (const auto* name : { "a", "b", "c", "d", "e" })
        stack.execute(record(log, name));
    log.clear();

    while (stack.undo())
    {
    }

    EXPECT_EQ(log, (std::vector<std::string>{ "undo e", "undo d", "undo c" }));
}

TEST(UndoStackTest, ShrinkingTheLimitTrimsExistingHistory)
{
    std::vector<std::string> log;
    UndoStack stack(10);
    for (const auto* name : { "a", "b", "c", "d" })
        stack.execute(record(log, name));

    stack.setMaxDepth(2);

    EXPECT_EQ(stack.undoDepth(), 2u);
}

TEST(UndoStackTest, ConsecutiveMergeableCommandsFormOneStep)
{
    int value = 0;
    UndoStack stack;

    stack.execute(std::make_unique<SetValueCommand>(value, 1));
    stack.execute(std::make_unique<SetValueCommand>(value, 2));
    stack.execute(std::make_unique<SetValueCommand>(value, 3));

    EXPECT_EQ(value, 3);
    EXPECT_EQ(stack.undoDepth(), 1u);

    stack.undo();
    EXPECT_EQ(value, 0);

    stack.redo();
    EXPECT_EQ(value, 3);
}

TEST(UndoStackTest, CommandsThatCannotMergeStaySeparate)
{
    std::vector<std::string> log;
    int value = 0;
    UndoStack stack;

    stack.execute(std::make_unique<SetValueCommand>(value, 1));
    stack.execute(record(log, "other"));
    stack.execute(std::make_unique<SetValueCommand>(value, 2));

    EXPECT_EQ(stack.undoDepth(), 3u);
}

TEST(UndoStackTest, UndoBreaksTheMergeChain)
{
    int value = 0;
    UndoStack stack;
    stack.execute(std::make_unique<SetValueCommand>(value, 1));
    stack.execute(std::make_unique<SetValueCommand>(value, 2));
    stack.execute(std::make_unique<SetValueCommand>(value, 5));
    stack.undo();
    stack.redo();

    stack.execute(std::make_unique<SetValueCommand>(value, 9));

    EXPECT_EQ(stack.undoDepth(), 2u);
    stack.undo();
    EXPECT_EQ(value, 5);
}

TEST(UndoStackTest, BreakMergeChainStartsANewStep)
{
    int value = 0;
    UndoStack stack;
    stack.execute(std::make_unique<SetValueCommand>(value, 1));

    stack.breakMergeChain();
    stack.execute(std::make_unique<SetValueCommand>(value, 2));

    EXPECT_EQ(stack.undoDepth(), 2u);
    stack.undo();
    EXPECT_EQ(value, 1);
}

TEST(UndoStackTest, FailingExecuteLeavesTheStackUntouched)
{
    std::vector<std::string> log;
    UndoStack stack;
    stack.execute(record(log, "a"));

    auto failing = std::make_unique<RecordingCommand>(log, "bad");
    failing->failOnExecute = true;

    EXPECT_THROW(stack.execute(std::move(failing)), std::runtime_error);
    EXPECT_EQ(stack.undoDepth(), 1u);
}

TEST(UndoStackTest, FailingUndoKeepsTheCommandOnTheStack)
{
    std::vector<std::string> log;
    UndoStack stack;
    auto failing = std::make_unique<RecordingCommand>(log, "bad");
    auto* failingPointer = failing.get();
    stack.execute(std::move(failing));
    failingPointer->failOnUndo = true;

    EXPECT_THROW(stack.undo(), std::runtime_error);

    EXPECT_EQ(stack.undoDepth(), 1u);
    EXPECT_FALSE(stack.canRedo());
}

TEST(UndoStackTest, ClearForgetsEverything)
{
    std::vector<std::string> log;
    UndoStack stack;
    stack.execute(record(log, "a"));
    stack.undo();
    stack.execute(record(log, "b"));

    stack.clear();

    EXPECT_FALSE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());
}

TEST(UndoStackTest, DescriptionsComeFromTheNextCommandToUndoOrRedo)
{
    std::vector<std::string> log;
    UndoStack stack;

    EXPECT_TRUE(stack.undoDescription().empty());

    stack.execute(record(log, "a", "Rename category"));
    stack.execute(record(log, "b", "Delete entry"));

    EXPECT_EQ(stack.undoDescription(), "Delete entry");

    stack.undo();

    EXPECT_EQ(stack.undoDescription(), "Rename category");
    EXPECT_EQ(stack.redoDescription(), "Delete entry");
}

TEST(UndoStackTest, ChangeSignalIsEmittedOncePerModification)
{
    std::vector<std::string> log;
    UndoStack stack;
    int emissions = 0;
    stack.signalChanged().connect([&emissions] { ++emissions; });

    stack.execute(record(log, "a"));
    stack.undo();
    stack.redo();
    stack.clear();

    EXPECT_EQ(emissions, 4);
}

TEST(UndoStackTest, UndoOnEmptyStackDoesNotEmit)
{
    UndoStack stack;
    int emissions = 0;
    stack.signalChanged().connect([&emissions] { ++emissions; });

    EXPECT_FALSE(stack.undo());
    EXPECT_FALSE(stack.redo());
    stack.clear();

    EXPECT_EQ(emissions, 0);
}

TEST(CompositeCommandTest, ExecutesInOrderAndUndoesInReverse)
{
    std::vector<std::string> log;
    auto composite = std::make_unique<CompositeCommand>("Import");
    composite->add(record(log, "a")).add(record(log, "b")).add(record(log, "c"));
    UndoStack stack;

    stack.execute(std::move(composite));
    stack.undo();

    EXPECT_EQ(log, (std::vector<std::string>{ "do a", "do b", "do c", "undo c", "undo b", "undo a" }));
    EXPECT_EQ(stack.redoDescription(), "Import");
}

TEST(CompositeCommandTest, FailureRollsBackTheCommandsAlreadyExecuted)
{
    std::vector<std::string> log;
    auto failing = std::make_unique<RecordingCommand>(log, "bad");
    failing->failOnExecute = true;

    CompositeCommand composite;
    composite.add(record(log, "a")).add(record(log, "b")).add(std::move(failing));

    EXPECT_THROW(composite.execute(), std::runtime_error);
    EXPECT_EQ(log, (std::vector<std::string>{ "do a", "do b", "undo b", "undo a" }));
}

TEST(CompositeCommandTest, EmptyCompositeDoesNothing)
{
    CompositeCommand composite("Nothing");

    composite.execute();
    composite.undo();

    EXPECT_EQ(composite.size(), 0u);
    EXPECT_EQ(composite.description(), "Nothing");
}
