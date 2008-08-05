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

#include <process.h>

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef testing::Test ObjectWatcherTest;

class QuitDelegate : public base::ObjectWatcher::Delegate {
 public:
  virtual void OnObjectSignaled(HANDLE object) {
    MessageLoop::current()->Quit();
  }
};

class DecrementCountDelegate : public base::ObjectWatcher::Delegate {
 public:
  DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  virtual void OnObjectSignaled(HANDLE object) {
    --(*counter_);
  }
 private:
  int* counter_;
};

}

TEST(ObjectWatcherTest, BasicSignal) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
  
  SetEvent(event);

  MessageLoop::current()->Run();

  CloseHandle(event);
}

TEST(ObjectWatcherTest, BasicCancel) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
  
  watcher.StopWatching();

  CloseHandle(event);
}


TEST(ObjectWatcherTest, CancelAfterSet) {
  base::ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

    // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
   
  SetEvent(event);
    
  // Let the background thread do its business
  Sleep(30);
    
  watcher.StopWatching();
  
  MessageLoop::current()->RunAllPending();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
  
  CloseHandle(event);
}

// Used so we can simulate a MessageLoop that dies before an ObjectWatcher.
// This ordinarily doesn't happen when people use the Thread class, but it can
// happen when people use the Singleton pattern or atexit.
static unsigned __stdcall ThreadFunc(void* param) {
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);  // not signaled
  {
    base::ObjectWatcher watcher;
    {
      MessageLoop message_loop;

      QuitDelegate delegate;
      watcher.StartWatching(event, &delegate);
    }
  }
  CloseHandle(event);
  return 0;
}

TEST(ObjectWatcherTest, OutlivesMessageLoop) {
  unsigned int thread_id;
  HANDLE thread = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     0,
                     ThreadFunc,
                     NULL,
                     0,
                     &thread_id));
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}
