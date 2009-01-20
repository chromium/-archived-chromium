// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/command_updater.h"

#include "base/logging.h"
#include "chrome/common/stl_util-inl.h"

CommandUpdater::CommandUpdater(CommandUpdaterDelegate* handler)
    : delegate_(handler) {
}

CommandUpdater::~CommandUpdater() {
  STLDeleteContainerPairSecondPointers(commands_.begin(), commands_.end());
}

bool CommandUpdater::IsCommandEnabled(int id) const {
  const CommandMap::const_iterator command(commands_.find(id));
  if (command == commands_.end())
    return false;
  return command->second->enabled;
}

bool CommandUpdater::SupportsCommand(int id) const {
  return commands_.find(id) != commands_.end();
}

void CommandUpdater::ExecuteCommand(int id) {
  if (IsCommandEnabled(id))
    delegate_->ExecuteCommand(id);
}

void CommandUpdater::UpdateCommandEnabled(int id, bool enabled) {
  Command* command = GetCommand(id, true);
  if (command->enabled == enabled)
    return;  // Nothing to do.
  command->enabled = enabled;
  FOR_EACH_OBSERVER(CommandObserver, command->observers,
                    EnabledStateChangedForCommand(id, enabled));
}

CommandUpdater::Command* CommandUpdater::GetCommand(int id, bool create) {
  bool supported = SupportsCommand(id);
  if (supported)
    return commands_[id];
  DCHECK(create);
  Command* command = new Command;
  commands_[id] = command;
  return command;
}

void CommandUpdater::AddCommandObserver(int id, CommandObserver* observer) {
  GetCommand(id, true)->observers.AddObserver(observer);
}

void CommandUpdater::RemoveCommandObserver(int id, CommandObserver* observer) {
  GetCommand(id, false)->observers.RemoveObserver(observer);
}
