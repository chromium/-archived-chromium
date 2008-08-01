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

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef testing::Test ObjectWatcherTest;

class DecrementCountTask : public Task {
 public:
  DecrementCountTask(int* counter) : counter_(counter) {
  }
  virtual void Run() {
    if (--(*counter_) == 0)
      MessageLoop::current()->Quit();
  }
 private:
  int* counter_;
};

}

TEST(ObjectWatcherTest, BasicSignal) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.AddWatch(FROM_HERE, event, new MessageLoop::QuitTask());
  EXPECT_TRUE(ok);
  
  SetEvent(event);

  MessageLoop::current()->Run();

  CloseHandle(event);
}

TEST(ObjectWatcherTest, BasicCancel) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.AddWatch(FROM_HERE, event, new MessageLoop::QuitTask());
  EXPECT_TRUE(ok);
  
  watcher.CancelWatch(event);

  CloseHandle(event);
}

TEST(ObjectWatcherTest, ManySignal) {
  base::ObjectWatcher watcher;

  const int kNumObjects = 2000;

  int counter = kNumObjects;
  HANDLE events[kNumObjects];
  
  for (int i = 0; i < kNumObjects; ++i) {
    // A manual-reset event that is not yet signaled.
    events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

    bool ok = watcher.AddWatch(
        FROM_HERE, events[i], new DecrementCountTask(&counter));
    EXPECT_TRUE(ok);
  }
   
  for (int i = 0; i < kNumObjects; ++i) 
    SetEvent(events[i]);

  MessageLoop::current()->Run();

  for (int i = 0; i < kNumObjects; ++i)
    CloseHandle(events[i]);
}

TEST(ObjectWatcherTest, ManyCancel) {
  base::ObjectWatcher watcher;

  const int kNumObjects = 2000;

  int counter = kNumObjects;
  HANDLE events[kNumObjects];
  
  for (int i = 0; i < kNumObjects; ++i) {
    // A manual-reset event that is not yet signaled.
    events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

    bool ok = watcher.AddWatch(
        FROM_HERE, events[i], new DecrementCountTask(&counter));
    EXPECT_TRUE(ok);
  }
   
  for (int i = 0; i < kNumObjects; ++i) 
    watcher.CancelWatch(events[i]);

  for (int i = 0; i < kNumObjects; ++i)
    CloseHandle(events[i]);
}

TEST(ObjectWatcherTest, CancelAfterSet) {
  base::ObjectWatcher watcher;

  const int kNumObjects = 50;

  int counter = kNumObjects;
  HANDLE events[kNumObjects];
  
  for (int i = 0; i < kNumObjects; ++i) {
    // A manual-reset event that is not yet signaled.
    events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

    bool ok = watcher.AddWatch(
        FROM_HERE, events[i], new DecrementCountTask(&counter));
    EXPECT_TRUE(ok);
  }
   
  for (int i = 0; i < kNumObjects; ++i) {
    SetEvent(events[i]);
    
    // Let the background thread do its business
    SleepEx(10, TRUE);
    
    // Occasionally pump some tasks.  Sometimes we cancel a watch after its
    // task has run and other times we do nothing.
    if (i % 3 == 0)
      MessageLoop::current()->RunAllPending();

    if (watcher.CancelWatch(events[i]))
      --counter;
  }
  
  EXPECT_EQ(0, counter);

  for (int i = 0; i < kNumObjects; ++i)
    CloseHandle(events[i]);
}
