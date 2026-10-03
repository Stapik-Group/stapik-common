#include "UndoActions.hpp"

#include <giomm/simpleaction.h>
#include <glibmm/variant.h>

UndoActions::UndoActions(Gio::ActionMap& actionMap, stapik::command::UndoStack& undoStack) :
    m_undoStack(undoStack),
    m_undoAction(Gio::SimpleAction::create("undo")),
    m_redoAction(Gio::SimpleAction::create("redo"))
{
    m_undoAction->signal_activate().connect([this](const Glib::VariantBase&) { m_undoStack.undo(); });
    m_redoAction->signal_activate().connect([this](const Glib::VariantBase&) { m_undoStack.redo(); });

    actionMap.add_action(m_undoAction);
    actionMap.add_action(m_redoAction);

    m_changeConnection = m_undoStack.signalChanged().connect([this] { updateEnabledState(); });

    updateEnabledState();
}

UndoActions::~UndoActions()
{
    m_changeConnection.disconnect();
}

void UndoActions::updateEnabledState()
{
    m_undoAction->set_enabled(m_undoStack.canUndo());
    m_redoAction->set_enabled(m_undoStack.canRedo());
}
