// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/command_observer_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

// Implements the callback interface. Records the last command id and
// enabled state it has received so it can be queried by the tests to see
// if we got a notification or not.
@interface CommandTestObserver : NSObject<CommandObserverProtocol> {
 @private
  int lastCommand_;  // id of last received state change
  bool lastState_;  // state of last received state change
}
- (int)lastCommand;
- (bool)lastState;
@end

@implementation CommandTestObserver
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  lastCommand_ = command;
  lastState_ = enabled;
}
- (int)lastCommand {
  return lastCommand_;
}
- (bool)lastState {
  return lastState_;
}
@end

namespace {

class CommandObserverBridgeTest : public testing::Test {
 public:
  CommandObserverBridgeTest()
      : updater_(new CommandUpdater(NULL)),
        observer_([[CommandTestObserver alloc] init]) {
  }
  scoped_ptr<CommandUpdater> updater_;
  scoped_nsobject<CommandTestObserver> observer_;
};

// Tests creation and deletion. NULL arguments aren't allowed.
TEST_F(CommandObserverBridgeTest, Create) {
  CommandObserverBridge bridge(observer_.get(), updater_.get());
}

// Observes state changes on command ids 1 and 2. Ensure we don't get
// a notification of a state change on a command we're not observing (3).
// Commands start off enabled in CommandUpdater.
TEST_F(CommandObserverBridgeTest, Observe) {
  CommandObserverBridge bridge(observer_.get(), updater_.get());
  bridge.ObserveCommand(1);
  bridge.ObserveCommand(2);

  // Validate initial state assumptions.
  EXPECT_EQ([observer_ lastCommand], 0);
  EXPECT_EQ([observer_ lastState], false);
  EXPECT_EQ(updater_->IsCommandEnabled(1), true);
  EXPECT_EQ(updater_->IsCommandEnabled(2), true);

  updater_->UpdateCommandEnabled(1, false);
  EXPECT_EQ([observer_ lastCommand], 1);
  EXPECT_EQ([observer_ lastState], false);

  updater_->UpdateCommandEnabled(2, false);
  EXPECT_EQ([observer_ lastCommand], 2);
  EXPECT_EQ([observer_ lastState], false);

  updater_->UpdateCommandEnabled(1, true);
  EXPECT_EQ([observer_ lastCommand], 1);
  EXPECT_EQ([observer_ lastState], true);

  // Change something we're not watching and make sure the last state hasn't
  // changed.
  updater_->UpdateCommandEnabled(3, false);
  EXPECT_EQ([observer_ lastCommand], 1);
  EXPECT_NE([observer_ lastCommand], 3);
  EXPECT_EQ([observer_ lastState], true);
}

}  // namespace
