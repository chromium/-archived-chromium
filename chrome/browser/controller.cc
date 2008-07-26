// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>

#include "chrome/browser/controller.h"
#include "base/logging.h"

CommandController::CommandController(CommandHandler* handler) :
    handler_(handler) {
}

CommandController::~CommandController() {
  // The button controllers are command observers hence this list
  // must be deleted before the commands below.
  std::vector<ButtonController*>::iterator bc_iter;
  for (bc_iter = managed_button_controllers_.begin();
       bc_iter != managed_button_controllers_.end();
       ++bc_iter)
    delete *bc_iter;

  CommandMap::iterator command;
  for (command = commands_.begin(); command != commands_.end(); ++command) {
    DCHECK(command->second);
    delete command->second->observers;
    delete command->second;
  }
}

bool CommandController::IsCommandEnabled(int id) const {
  const CommandMap::const_iterator command(commands_.find(id));
  if (command == commands_.end())
    return false;

  DCHECK(command->second);
  return command->second->enabled;
}

bool CommandController::SupportsCommand(int id) const {
  return commands_.find(id) != commands_.end();
}

bool CommandController::GetContextualLabel(int id, std::wstring* out) const {
  return handler_->GetContextualLabel(id, out);
}

void CommandController::ExecuteCommand(int id) {
  if (IsCommandEnabled(id))
    handler_->ExecuteCommand(id);
}

void CommandController::UpdateCommandEnabled(int id, bool enabled) {
  Command* command = GetCommand(id, true);
  if (command->enabled == enabled)
    return;  // Nothing to do.
  command->enabled = enabled;
  CommandObserverList* list = command->observers;

  // FIXME: If Update calls RemoveCommandObserver, the iterator will be
  //        invalidated. Darin says he's working on a new ObserverList
  //        class that will handle this properly. For right now, don't
  //        do that!
  CommandObserverList::const_iterator end = list->end();
  CommandObserverList::iterator observer = list->begin();
  while (observer != end)
    (*observer++)->SetEnabled(enabled);
}

Command* CommandController::GetCommand(int id, bool create) {
  Command* command = NULL;
  bool supported = SupportsCommand(id);
  if (supported) {
    command = commands_[id];
  } else if (create) {
    command = new Command;
    DCHECK(command) << "Controller::GetCommand - OOM!";
    command->observers = new CommandObserverList;
    DCHECK(command->observers) << "Controller::GetCommand - OOM!";
    command->enabled = false;
    commands_[id] = command;
  }
  return command;
}

void CommandController::AddCommandObserver(int id, CommandObserver* observer) {
  Command* command = GetCommand(id, true);
  CommandObserverList* list = command->observers;

  CommandObserverList::const_iterator end = list->end();
  CommandObserverList::iterator existing =
      find(list->begin(), list->end(), observer);
  if (existing != end)
    return;

  list->push_back(observer);
}

void CommandController::RemoveCommandObserver(int id,
                                              CommandObserver* observer) {
  Command* command = GetCommand(id, false);
  if (!command)
    return;

  CommandObserverList* list = command->observers;
  CommandObserverList::const_iterator end = list->end();
  CommandObserverList::iterator existing =
      find(list->begin(), list->end(), observer);

  if (existing != end)
    list->erase(existing);
}

void CommandController::AddManagedButton(ChromeViews::Button* b, int command) {
  ButtonController* bc = new ButtonController(b, this, command);
  managed_button_controllers_.push_back(bc);
}
