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

#include "sandbox/src/win2k_threadpool.h"
#include "testing/gtest/include/gtest/gtest.h"

void __stdcall EmptyCallBack(void*, unsigned char) {
}

void __stdcall TestCallBack(void* context, unsigned char) {
  HANDLE event = reinterpret_cast<HANDLE>(context);
  ::SetEvent(event);
}

namespace sandbox {

// Test that register and unregister work, part 1.
TEST(IPCTest, ThreadPoolRegisterTest1) {
  Win2kThreadPool thread_pool;

  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  uint32 context = 0;
  EXPECT_FALSE(thread_pool.RegisterWait(0, event1, EmptyCallBack, &context));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, EmptyCallBack, &context));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(this, event2, EmptyCallBack, &context));
  EXPECT_EQ(2, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

// Test that register and unregister work, part 2.
TEST(IPCTest, ThreadPoolRegisterTest2) {
  Win2kThreadPool thread_pool;

  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  uint32 context = 0;
  uint32 c1 = 0;
  uint32 c2 = 0;

  EXPECT_TRUE(thread_pool.RegisterWait(&c1, event1, EmptyCallBack, &context));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(&c2, event2, EmptyCallBack, &context));
  EXPECT_EQ(2, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c1));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

// Test that the thread pool has at least a thread that services an event.
// Test that when the event is un-registered is no longer serviced.
TEST(IPCTest, ThreadPoolSignalAndWaitTest) {
  Win2kThreadPool thread_pool;

  // The events are auto reset and start not signaled.
  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, TestCallBack, event2));

  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, FALSE));
  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, FALSE));

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(WAIT_TIMEOUT, ::SignalObjectAndWait(event1, event2, 1000, FALSE));

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

}  // namespace sandbox
