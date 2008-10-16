// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLER_H__
#define CHROME_BROWSER_CONTROLLER_H__

#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "chrome/views/button.h"
#include "chrome/views/controller.h"

class ButtonController;

///////////////////////////////////////////////////////////////////////////////
//
// CommandObserver interface
//
//   A component of the View portion of the MVC pattern implements the
//   CommandObserver interface to update itself when the state of its command
//   changes.
//
///////////////////////////////////////////////////////////////////////////////
class CommandObserver {
 public:
  // Update the View because this command's enabled state has changed.
  virtual void SetEnabled(bool enabled) = 0;
};

///////////////////////////////////////////////////////////////////////////////
//
// CommandHandler interface
//
//   An object implementing the CommandHandler interface is responsible for
//   actually executing specific commands.
//
//   This object is also responsible for producing contextual labels if needed.
//
///////////////////////////////////////////////////////////////////////////////
class CommandHandler {
 public:

  // This method is called to give the command handler a chance to specify
  // a contextual label for the provided command id. Returns true if a
  // contextual label has been assigned or false if the default label should be
  // used.
  virtual bool GetContextualLabel(int id, std::wstring* out) const {
    return false;
  }

  // Whether the specified command can be executed.
  virtual bool IsCommandEnabled(int id) const { return true; }

  // Execute a command, according to the command's state (currently binary!)
  virtual void ExecuteCommand(int id) = 0;
};

typedef std::vector<CommandObserver*> CommandObserverList;

// A piece of data about a command - whether or not it is enabled, and a list
// of objects that observe the enabled state of this command.
struct Command {
  bool enabled;
  CommandObserverList* observers;
};
typedef base::hash_map<int, Command*> CommandMap;


///////////////////////////////////////////////////////////////////////////////
//
// Controller class
//
//   This is the Controller portion of a MVC pattern. It handles dispatching
//   commands, maintaining enabled state, and updating the UI as that state
//   changes. The purpose of using MVC and a controller like this is to
//   maintain a clear separation between rendering, control logic and various
//   data sources so that code is more maintainable.
//
///////////////////////////////////////////////////////////////////////////////
class CommandController : public Controller {
 public:
  // The controller is constructed with an object implementing the
  // CommandHandler interface, to which the Controller defers execution
  // duties. This keeps the Controller fairly simple without requiring a
  // lot of reworking of the command handlers. If there are significant
  // groups of commands that require execution separated from this handler,
  // then the Command object can be extended to provide a handler field that
  // specifies a handler different to the default.
  CommandController(CommandHandler* handler);
  virtual ~CommandController();

  // Add a button to the list of managed buttons. The button is synced with
  // the provided command
  void AddManagedButton(views::Button* button, int command);

  // Controller
  virtual bool SupportsCommand(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual bool GetContextualLabel(int id, std::wstring* out) const;
  virtual void ExecuteCommand(int id);


  // Adds an observer to the state of a particular command. If the command does
  // not exist, it is created, initialized to false.
  void AddCommandObserver(int id, CommandObserver* observer);

  // Removes an observer to the state of a particular command.
  void RemoveCommandObserver(int id, CommandObserver* observer);

  // Notify all observers of a particular command that the command has been
  // enabled or disabled. If the command does not exist, it is created and
  // initialized to |state|. This function is very lightweight if the command
  // state has not changed.
  void UpdateCommandEnabled(int id, bool state);

 private:
  ///////////////////////////////////////////////////////////////////////////////
  //
  // ButtonController
  //
  // An adapter class to use views buttons with our controller
  //
  ///////////////////////////////////////////////////////////////////////////////
  class ButtonController : public views::BaseButton::ButtonListener,
                           public CommandObserver {

   public:

    ButtonController(views::Button* b,
                     CommandController* controller,
                     int command)
        : button_(b),
          controller_(controller) {
      controller_->AddCommandObserver(command, this);
      button_->SetListener(this, command);

      // The button's initial state should be the current command state.
      button_->SetEnabled(controller_->IsCommandEnabled(command));
    }

    virtual void SetEnabled(bool enabled) {
      button_->SetEnabled(enabled);
    }

    virtual void ButtonPressed(views::BaseButton* sender) {
      controller_->ExecuteCommand(sender->GetTag());
    }

   private:
    views::Button* button_;
    CommandController* controller_;
  };

  // Get a Command node for a given command ID, creating an entry if it doesn't
  // exist if desired.
  Command* GetCommand(int id, bool create);

  // This is the default handler for all command execution
  CommandHandler* handler_;

  // This is a map of command IDs to states and observer lists
  CommandMap commands_;

  // vector of ButtonController for managed buttons
  std::vector<ButtonController*> managed_button_controllers_;

  CommandController();
  DISALLOW_EVIL_CONSTRUCTORS(CommandController);
};

#endif // CHROME_BROWSER_CONTROLLER_H__
