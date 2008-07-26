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
