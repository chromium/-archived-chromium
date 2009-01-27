// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"


class RenderThreadTest : public testing::Test {
 public:
  virtual void SetUp() {
    // Must have a message loop to create a RenderThread()
    message_loop_ = new MessageLoop(MessageLoop::TYPE_DEFAULT);
  }
  
  virtual void TearDown() {
    delete message_loop_;
  }

 private:
  MessageLoop *message_loop_;
  
};


TEST_F(RenderThreadTest, TestGlobal) {
  ASSERT_FALSE(g_render_thread);
  {
    RenderThread thread(EmptyWString());
    ASSERT_TRUE(g_render_thread);
  }
  ASSERT_FALSE(g_render_thread);
}

TEST_F(RenderThreadTest, TestVisitedMsg) {
  RenderThread thread(EmptyWString());
  IPC::Message* msg = new ViewMsg_VisitedLink_NewTable(NULL);
  ASSERT_TRUE(msg);
  // Message goes nowhere, but this confirms Init() has happened.
  // Unusually (?), RenderThread() Start()s itself in it's constructor.
  thread.Send(msg);
  delete msg;
}

