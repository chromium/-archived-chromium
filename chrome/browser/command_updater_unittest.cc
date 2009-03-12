// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/command_updater.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingCommandHandlerMock
    : public CommandUpdater::CommandUpdaterDelegate {
 public:
  virtual void ExecuteCommand(int id) {
    EXPECT_EQ(1, id);
  }
};

class CommandUpdaterTest : public testing::Test {
};

class TestingCommandObserverMock : public CommandUpdater::CommandObserver {
 public:
  TestingCommandObserverMock() : enabled_(true) {}

  virtual void EnabledStateChangedForCommand(int id, bool enabled) {
    enabled_ = enabled;
  }

  bool enabled() const { return enabled_; }

 private:
  bool enabled_;
};

TEST_F(CommandUpdaterTest, TestBasicAPI) {
  TestingCommandHandlerMock handler;
  CommandUpdater command_updater(&handler);

  // Unsupported command
  EXPECT_FALSE(command_updater.SupportsCommand(0));
  EXPECT_FALSE(command_updater.IsCommandEnabled(0));
  // TestingCommandHandlerMock::ExecuteCommand should not be called, since
  // the command is not supported.
  command_updater.ExecuteCommand(0);

  // Supported, enabled command
  command_updater.UpdateCommandEnabled(1, true);
  EXPECT_TRUE(command_updater.SupportsCommand(1));
  EXPECT_TRUE(command_updater.IsCommandEnabled(1));
  command_updater.ExecuteCommand(1);

  // Supported, disabled command
  command_updater.UpdateCommandEnabled(2, false);
  EXPECT_TRUE(command_updater.SupportsCommand(2));
  EXPECT_FALSE(command_updater.IsCommandEnabled(2));
  // TestingCommandHandlerMock::ExecuteCommmand should not be called, since
  // the command_updater is disabled
  command_updater.ExecuteCommand(2);
}

TEST_F(CommandUpdaterTest, TestObservers) {
  TestingCommandHandlerMock handler;
  CommandUpdater command_updater(&handler);

  // Create an observer for the command 2 and add it to the controller, then
  // update the command.
  TestingCommandObserverMock observer;
  command_updater.AddCommandObserver(2, &observer);
  command_updater.UpdateCommandEnabled(2, true);
  EXPECT_TRUE(observer.enabled());
  command_updater.UpdateCommandEnabled(2, false);
  EXPECT_FALSE(observer.enabled());

  // Remove the observer and update the command.
  command_updater.RemoveCommandObserver(2, &observer);
  command_updater.UpdateCommandEnabled(2, true);
  EXPECT_FALSE(observer.enabled());
}

TEST_F(CommandUpdaterTest, TestObserverRemovingAllCommands) {
  TestingCommandHandlerMock handler;
  CommandUpdater command_updater(&handler);

  // Create two observers for the commands 1-3 as true, remove one using the
  // single remove command, then set the command to false. Ensure that the
  // removed observer still thinks all commands are true and the one left
  // observing picked up the change.

  TestingCommandObserverMock observer_remove, observer_keep;
  command_updater.AddCommandObserver(1, &observer_remove);
  command_updater.AddCommandObserver(2, &observer_remove);
  command_updater.AddCommandObserver(3, &observer_remove);
  command_updater.AddCommandObserver(1, &observer_keep);
  command_updater.AddCommandObserver(2, &observer_keep);
  command_updater.AddCommandObserver(3, &observer_keep);
  command_updater.UpdateCommandEnabled(1, true);
  command_updater.UpdateCommandEnabled(2, true);
  command_updater.UpdateCommandEnabled(3, true);
  EXPECT_TRUE(observer_remove.enabled());

  // Remove one observer and update the command. Check the states, which
  // should be different.
  command_updater.RemoveCommandObserver(&observer_remove);
  command_updater.UpdateCommandEnabled(1, false);
  command_updater.UpdateCommandEnabled(2, false);
  command_updater.UpdateCommandEnabled(3, false);
  EXPECT_TRUE(observer_remove.enabled());
  EXPECT_FALSE(observer_keep.enabled());
}
