// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/sys_info.h"
#include "base/string_util.h"
#include "chrome/renderer/render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kThreadName[] = "render_process_unittest";

class RenderProcessTest : public testing::Test {
 public:
  virtual void SetUp() {
    // Need a MODE_SERVER to make MODE_CLIENTs (like a RenderThread) happy.
    channel_ = new IPC::Channel(kThreadName, IPC::Channel::MODE_SERVER, NULL);
    render_process_.reset(new RenderProcess(kThreadName));
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    render_process_.reset();
    // Need to fully destruct IPC::SyncChannel before the message loop goes
    // away.
    message_loop_.RunAllPending();
    // Delete the server channel after the RenderThread so that
    // IPC::SyncChannel's OnChannelError doesn't fire on the context and attempt
    // to use the listener thread which is now gone.
    delete channel_;
  }

 private:
  MessageLoopForIO message_loop_;
  scoped_ptr<RenderProcess> render_process_;
  IPC::Channel *channel_;
};


TEST_F(RenderProcessTest, TestTransportDIBAllocation) {
  // On Mac, we allocate in the browser so this test is invalid.
#if !defined(OS_MACOSX)
  const gfx::Rect rect(0, 0, 100, 100);
  TransportDIB* dib;
  skia::PlatformCanvas* canvas =
      RenderProcess::current()->GetDrawingCanvas(&dib, rect);
  ASSERT_TRUE(dib);
  ASSERT_TRUE(canvas);
  RenderProcess::current()->ReleaseTransportDIB(dib);
  delete canvas;
#endif
}

}  // namespace
