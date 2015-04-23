// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/command_observer_bridge.h"

#include "base/logging.h"

CommandObserverBridge::CommandObserverBridge(
    id<CommandObserverProtocol> observer, CommandUpdater* commands)
        : observer_(observer), commands_(commands) {
  DCHECK(observer_ && commands_);
}

CommandObserverBridge::~CommandObserverBridge() {
  // Unregister the notifications
  commands_->RemoveCommandObserver(this);
}

void CommandObserverBridge::ObserveCommand(int command) {
  commands_->AddCommandObserver(command, this);
}

void CommandObserverBridge::EnabledStateChangedForCommand(int command,
                                                          bool enabled) {
  [observer_ enabledStateChangedForCommand:command
                                   enabled:enabled ? YES : NO];
}
