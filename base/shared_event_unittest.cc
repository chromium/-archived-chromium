// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <process.h>  // _beginthreadex

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/shared_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SharedEventTest : public testing::Test {
};

Lock lock;

unsigned __stdcall MultipleThreadMain(void* param) {
  SharedEvent* shared_event = reinterpret_cast<SharedEvent*>(param);
  AutoLock l(lock);
  shared_event->SetSignaledState(!shared_event->IsSignaled());
  return 0;
}

}

TEST(SharedEventTest, ThreadSignaling) {
  // Create a set of 5 threads to each open a shared event and flip the
  // signaled state.  Verify that when the threads complete, the final state is
  // not-signaled.
  // I admit this doesn't test much, but short of spawning separate processes
  // and using IPC with a SharedEventHandle, there's not much to unittest.
  const int kNumThreads = 5;
  HANDLE threads[kNumThreads];

  scoped_ptr<SharedEvent> shared_event(new SharedEvent);
  shared_event->Create(true, true);

  // Spawn the threads.
  for (int16 index = 0; index < kNumThreads; index++) {
    void *argument = reinterpret_cast<void*>(shared_event.get());
    unsigned thread_id;
    threads[index] = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, MultipleThreadMain, argument, 0, &thread_id));
    EXPECT_NE(threads[index], static_cast<HANDLE>(NULL));
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    DWORD rv = WaitForSingleObject(threads[index], 60*1000);
    EXPECT_EQ(rv, WAIT_OBJECT_0);  // verify all threads finished
    CloseHandle(threads[index]);
  }
  EXPECT_FALSE(shared_event->IsSignaled());
}

