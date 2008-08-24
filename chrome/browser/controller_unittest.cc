// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controller.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingCommandHandlerMock : public CommandHandler {
 public:
  virtual void ExecuteCommand(int id) {
    EXPECT_EQ(1, id);
  }
};

class CommandControllerTest : public testing::Test {
 public:
  CommandControllerTest() : controller_(&handler_) { }
 protected:
  CommandController controller_;
  TestingCommandHandlerMock handler_;
};

class TestingCommandObserverMock : public CommandObserver {
 public:
  virtual void SetEnabled(bool enabled) {
    enabled_ = enabled;
  }

  bool enabled() {
    return enabled_;
  }

 private:
  bool enabled_;
};

TEST_F(CommandControllerTest, TestBasicAPI) {
  // Unsupported command
  EXPECT_FALSE(controller_.SupportsCommand(0));
  EXPECT_FALSE(controller_.IsCommandEnabled(0));
  // TestingCommandHandlerMock::ExecuteCommand should not be called, since
  // the command is not supported.
  controller_.ExecuteCommand(0);

  // Supported, enabled command
  controller_.UpdateCommandEnabled(1, true);
  EXPECT_TRUE(controller_.SupportsCommand(1));
  EXPECT_TRUE(controller_.IsCommandEnabled(1));
  controller_.ExecuteCommand(1);

  // Supported, disabled command
  controller_.UpdateCommandEnabled(2, false);
  EXPECT_TRUE(controller_.SupportsCommand(2));
  EXPECT_FALSE(controller_.IsCommandEnabled(2));
  // TestingCommandHandlerMock::ExecuteCommmand should not be called, since
  // the command is disabled
  controller_.ExecuteCommand(2);
}

TEST_F(CommandControllerTest, TestObservers) {
  TestingCommandHandlerMock handler;
  CommandController controller(&handler);

  // Create an observer for the command 2 and add it to the controller, then
  // update the command.
  TestingCommandObserverMock observer;
  controller_.AddCommandObserver(2, &observer);
  controller_.UpdateCommandEnabled(2, true);
  EXPECT_TRUE(observer.enabled());
  controller_.UpdateCommandEnabled(2, false);
  EXPECT_FALSE(observer.enabled());

  // Remove the observer and update the command.
  controller_.RemoveCommandObserver(2, &observer);
  controller_.UpdateCommandEnabled(2, true);
  EXPECT_FALSE(observer.enabled());
}

TEST_F(CommandControllerTest, TestRemoveObserverForUnsupportedCommand) {
  TestingCommandHandlerMock handler;
  CommandController controller(&handler);

  // Test removing observers for commands that are unsupported
  TestingCommandObserverMock observer;
  controller_.RemoveCommandObserver(3, &observer);
}

TEST_F(CommandControllerTest, TestAddingNullObserver) {
  TestingCommandHandlerMock handler;
  CommandController controller(&handler);

  // Test adding/removing NULL observers
  controller_.AddCommandObserver(4, NULL);
}

TEST_F(CommandControllerTest, TestRemovingNullObserver) {
  TestingCommandHandlerMock handler;
  CommandController controller(&handler);

  controller_.RemoveCommandObserver(4, NULL);
}

