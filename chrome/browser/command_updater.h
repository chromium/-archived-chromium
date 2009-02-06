// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMAND_UPDATER_H_
#define CHROME_BROWSER_COMMAND_UPDATER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"

////////////////////////////////////////////////////////////////////////////////
//
// CommandUpdater class
//
//   This object manages the enabled state of a set of commands. Observers
//   register to listen to changes in this state so they can update their
//   presentation.
//
class CommandUpdater {
 public:
  // A Delegate object implements this interface so that it can execute commands
  // when needed.
  class CommandUpdaterDelegate {
   public:
    // Perform the action associated with the command with the specified ID.
    virtual void ExecuteCommand(int id) = 0;
  };

  // Create a CommandUpdater with a CommandUpdaterDelegate to handle execution
  // of specific commands.
  explicit CommandUpdater(CommandUpdaterDelegate* handler);
  virtual ~CommandUpdater();

  // Returns true if the specified command ID is supported.
  bool SupportsCommand(int id) const;

  // Returns true if the specified command ID is enabled. The command ID must be
  // supported by this updater.
  bool IsCommandEnabled(int id) const;

  // Performs the action associated with this command ID.
  // TODO(beng): get rid of this since it's effectively just a pass-thru and the
  // call sites would be better off using more well defined delegate interfaces.
  void ExecuteCommand(int id);

  // An Observer interface implemented by objects that want to be informed when
  // the state of a particular command ID is modified.
  class CommandObserver {
   public:
    // Notifies the observer that the enabled state has changed for the
    // specified command id.
    virtual void EnabledStateChangedForCommand(int id, bool enabled) = 0;
  };

  // Adds an observer to the state of a particular command. If the command does
  // not exist, it is created, initialized to false.
  void AddCommandObserver(int id, CommandObserver* observer);

  // Removes an observer to the state of a particular command.
  void RemoveCommandObserver(int id, CommandObserver* observer);
  
  // Removes |observer| for all commands on which it's registered.
  void RemoveCommandObserver(CommandObserver* observer);

  // Notify all observers of a particular command that the command has been
  // enabled or disabled. If the command does not exist, it is created and
  // initialized to |state|. This function is very lightweight if the command
  // state has not changed.
  void UpdateCommandEnabled(int id, bool state);

 private:
  // A piece of data about a command - whether or not it is enabled, and a list
  // of objects that observe the enabled state of this command.
  class Command {
   public:
    bool enabled;
    ObserverList<CommandObserver> observers;

    Command() : enabled(true) {}
  };

  // Get a Command node for a given command ID, creating an entry if it doesn't
  // exist if desired.
  Command* GetCommand(int id, bool create);

  // The delegate is responsible for executing commands.
  CommandUpdaterDelegate* delegate_;

  // This is a map of command IDs to states and observer lists
  typedef base::hash_map<int, Command*> CommandMap;
  CommandMap commands_;

  CommandUpdater();
  DISALLOW_EVIL_CONSTRUCTORS(CommandUpdater);
};

#endif // CHROME_BROWSER_COMMAND_UPDATER_H_
