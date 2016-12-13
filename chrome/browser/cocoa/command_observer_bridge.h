// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_COMMAND_OBSERVER_BRIDGE
#define CHROME_BROWSER_COCOA_COMMAND_OBSERVER_BRIDGE

#import <Cocoa/Cocoa.h>

#include "chrome/browser/command_updater.h"

@protocol CommandObserverProtocol;

// A C++ bridge class that handles listening for updates to commands and
// passing them back to an object that supports the protocol delcared below.
// The observer will create one of these bridges, call ObserveCommand() on the
// command ids it cares about, and then wait for update notifications,
// delivered via -enabledStateChangedForCommand:enabled:. Destroying this
// bridge will handle automatically unregistering for updates, so there's no
// need to do that manually.

class CommandObserverBridge : public CommandUpdater::CommandObserver {
 public:
  CommandObserverBridge(id<CommandObserverProtocol> observer,
                        CommandUpdater* commands);
  virtual ~CommandObserverBridge();

  // Register for updates about |command|.
  void ObserveCommand(int command);

 protected:
  // Overridden from CommandUpdater::CommandObserver
  virtual void EnabledStateChangedForCommand(int command, bool enabled);

 private:
  id<CommandObserverProtocol> observer_;  // weak, owns me
  CommandUpdater* commands_;  // weak
};

// Implemented by the observing Objective-C object, called when there is a
// state change for the given command.
@protocol CommandObserverProtocol
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled;
@end

#endif  // CHROME_BROWSER_COCOA_COMMAND_OBSERVER_BRIDGE
