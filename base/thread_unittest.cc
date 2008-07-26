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

#include "base/lock.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class ThreadTest : public testing::Test {
  };
}

class ToggleValue : public Task {
 public:
  explicit ToggleValue(bool* value) : value_(value) {
  }
  virtual void Run() {
    *value_ = !*value_;
  }
 private:
  bool* value_;
};

class SleepSome : public Task {
 public:
  explicit SleepSome(DWORD msec) : msec_(msec) {
  }
  virtual void Run() {
    Sleep(msec_);
  }
 private:
  DWORD msec_;
};

TEST(ThreadTest, BasicTest1) {
  Thread a("BasicTest1");
  a.Start();
  EXPECT_TRUE(a.message_loop());
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  a.Start();
  EXPECT_TRUE(a.message_loop());
  a.Stop();
  EXPECT_FALSE(a.message_loop());
}

TEST(ThreadTest, BasicTest2) {
  Thread a("BasicTest2");
  a.Start();
  EXPECT_TRUE(a.message_loop());

  bool was_invoked = false;
  a.message_loop()->PostTask(FROM_HERE, new ToggleValue(&was_invoked));

  // wait for the task to run (we could use a kernel event here
  // instead to avoid busy waiting, but this is sufficient for
  // testing purposes).
  for (int i = 50; i >= 0 && !was_invoked; --i) {
    Sleep(20);
  }
  EXPECT_TRUE(was_invoked);
}

TEST(ThreadTest, BasicTest3) {
  bool was_invoked = false;
  {
    Thread a("BasicTest3");
    a.Start();
    EXPECT_TRUE(a.message_loop());

    // Test that all events are dispatched before the Thread object is
    // destroyed.  We do this by dispatching a sleep event before the
    // event that will toggle our sentinel value.
    a.message_loop()->PostTask(FROM_HERE, new SleepSome(500));
    a.message_loop()->PostTask(FROM_HERE, new ToggleValue(&was_invoked));
  }
  EXPECT_TRUE(was_invoked);
}

namespace {
  class DummyTask : public Task {
   public:
    explicit DummyTask(int* counter) : counter_(counter) {
    }
    void Run() {
      // Let's make sure that no other thread is running while we
      // are executing this code.  The sleeps help us assert that.
      int initial_value = *counter_;
      Sleep(1);
      ++(*counter_);
      Sleep(1);
      int final_value = *counter_;
      DCHECK(final_value == initial_value + 1);
    }
   private:
    int* counter_;
  };
}

namespace {
  // This task just continuously posts events to its thread, keeping it well
  // saturated with work.  If our thread interlocking is not fair, then we will
  // never exit.
  class BusyTask : public Task {
   public:
    explicit BusyTask(HANDLE quit_event) : quit_event_(quit_event) {
    }
    void Run() {
      if (WaitForSingleObject(quit_event_, 0) != WAIT_OBJECT_0)
        MessageLoop::current()->PostTask(FROM_HERE, new BusyTask(quit_event_));
    }
   private:
    HANDLE quit_event_;
  };

  // This task just tries to set the quit sentinel to signal the busy thread
  // to stop doing work.
  class InterruptBusyTask : public Task {
   public:
    explicit InterruptBusyTask(HANDLE quit_event) : quit_event_(quit_event) {
    }
    void Run() {
      SetEvent(quit_event_);
    }
   private:
    HANDLE quit_event_;
  };
}

