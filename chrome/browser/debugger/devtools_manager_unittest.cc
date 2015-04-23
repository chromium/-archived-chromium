// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/common/render_messages.h"

namespace {

class TestDevToolsClientHost : public DevToolsClientHost {
 public:
  TestDevToolsClientHost()
      : last_sent_message(NULL),
        closed_(false) {
  }

  virtual ~TestDevToolsClientHost() {
    EXPECT_TRUE(closed_);
  }

  virtual void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    NotifyCloseListener();
    closed_ = true;
  }
  virtual void InspectedTabClosing() {
    Close();
  }

  virtual void SetInspectedTabUrl(const std::string& url) {
  }

  virtual void SendMessageToClient(const IPC::Message& message) {
    last_sent_message = &message;
  }

  static void ResetCounters() {
    close_counter = 0;
  }

  static int close_counter;

  const IPC::Message* last_sent_message;

 private:
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};

int TestDevToolsClientHost::close_counter = 0;

}  // namespace

class DevToolsManagerTest : public RenderViewHostTestHarness {
 public:
  DevToolsManagerTest() : RenderViewHostTestHarness() {
  }

 protected:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
  }
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsClientHost) {
  scoped_refptr<DevToolsManager> manager = new DevToolsManager();

  DevToolsClientHost* host = manager->GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(NULL == host);

  TestDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(rvh(), &client_host);
  // Test that just registered devtools host is returned.
  host = manager->GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  // Test that the same devtools host is returned.
  host = manager->GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  host = manager->GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(NULL == host);
}

TEST_F(DevToolsManagerTest, ForwardMessageToClient) {
  scoped_refptr<DevToolsManager> manager = new DevToolsManager();

  TestDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(rvh(), &client_host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  IPC::Message m;
  manager->ForwardToDevToolsClient(rvh(), m);
  EXPECT_TRUE(&m == client_host.last_sent_message);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
}
