// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/waitable_event.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kThreadName[] = L"render_thread_unittest";

class RenderThreadTest : public testing::Test {
 public:
  virtual void SetUp() {
    mock_process_.reset(new MockProcess(new RenderThread(kThreadName)));
    // Need a MODE_SERVER to make MODE_CLIENTs (like a RenderThread) happy.
    channel_ = new IPC::Channel(kThreadName, IPC::Channel::MODE_SERVER, NULL);
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    delete channel_;
    mock_process_.reset();
  }

 protected:
  MessageLoopForIO message_loop_;
  scoped_ptr<MockProcess> mock_process_;
  IPC::Channel *channel_;
};

TEST_F(RenderThreadTest, TestGlobal) {
  ASSERT_TRUE(RenderThread::current());
}

TEST_F(RenderThreadTest, TestVisitedMsg) {
#if defined(OS_WIN)
  IPC::Message* msg = new ViewMsg_VisitedLink_NewTable(NULL);
#elif defined(OS_POSIX)
  IPC::Message* msg = new ViewMsg_VisitedLink_NewTable(
      base::SharedMemoryHandle(0, false));
#endif
  ASSERT_TRUE(msg);
  // Message goes nowhere, but this confirms Init() has happened.
  // Unusually (?), RenderThread() Start()s itself in it's constructor.
  mock_process_->child_thread()->Send(msg);

  // No need to delete msg; per Message::Send() documentation, "The
  // implementor takes ownership of the given Message regardless of
  // whether or not this method succeeds."
}

}  // namespace
