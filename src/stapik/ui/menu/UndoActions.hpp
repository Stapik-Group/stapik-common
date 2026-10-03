#pragma once

#include "stapik/command/UndoStack.hpp"

#include <giomm/actionmap.h>
#include <giomm/simpleaction.h>
#include <sigc++/connection.h>

class UndoActions
{
public:
    UndoActions(Gio::ActionMap& actionMap, stapik::command::UndoStack& undoStack);
    ~UndoActions();

    UndoActions(const UndoActions&) = delete;
    UndoActions& operator=(const UndoActions&) = delete;

private:
    void updateEnabledState() const;

    stapik::command::UndoStack& m_undoStack;
    Glib::RefPtr<Gio::SimpleAction> m_undoAction;
    Glib::RefPtr<Gio::SimpleAction> m_redoAction;
    sigc::connection m_changeConnection;
};
