// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// This is important because if there are 2 different message loops, we must
// have 2 different WebKit threads which would be very bad.
TEST(WebKitThreadTest, TwoThreadsShareMessageLoopTest) {
  scoped_refptr<WebKitThread> thread_a = new WebKitThread;
  scoped_refptr<WebKitThread> thread_b = new WebKitThread;
  MessageLoop* loop_a = thread_a->GetMessageLoop();
  MessageLoop* loop_b = thread_b->GetMessageLoop();
  ASSERT_FALSE(loop_a == NULL);
  ASSERT_EQ(loop_a, loop_b);
}
