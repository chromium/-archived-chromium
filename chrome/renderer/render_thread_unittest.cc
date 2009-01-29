// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kThreadName[] = "render_thread_unittest";

class RenderThreadTest : public testing::Test {
 public:
  virtual void SetUp() {
    MockProcess::GlobalInit();
    // Need a MODE_SERVER to make MODE_CLIENTs (like a RenderThread) happy.
    channel_ = new IPC::Channel(ASCIIToWide(kThreadName),
                                IPC::Channel::MODE_SERVER, NULL);
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    delete channel_;
    MockProcess::GlobalCleanup();
  }

 private:
  MessageLoopForIO message_loop_;
  IPC::Channel *channel_;
};

TEST_F(RenderThreadTest, TestGlobal) {
  ASSERT_FALSE(g_render_thread);
  {
    RenderThread thread(ASCIIToWide(kThreadName));
    ASSERT_TRUE(g_render_thread);
  }
  ASSERT_FALSE(g_render_thread);
}

TEST_F(RenderThreadTest, TestVisitedMsg) {
  RenderThread thread(ASCIIToWide(kThreadName));
  IPC::Message* msg = new ViewMsg_VisitedLink_NewTable(NULL);
  ASSERT_TRUE(msg);
  // Message goes nowhere, but this confirms Init() has happened.
  // Unusually (?), RenderThread() Start()s itself in it's constructor.
  thread.Send(msg);

  // No need to delete msg; per Message::Send() documentation, "The
  // implementor takes ownership of the given Message regardless of
  // whether or not this method succeeds."
}

}  // namespace
