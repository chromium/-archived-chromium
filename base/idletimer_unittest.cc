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

#include "base/idle_timer.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class IdleTimerTest : public testing::Test {
  };
};

// We Mock the GetLastInputInfo function to return
// the time stored here.
static DWORD mock_idle_time = GetTickCount();

BOOL __stdcall MockGetLastInputInfoFunction(PLASTINPUTINFO plii) {
  DCHECK(plii->cbSize == sizeof(LASTINPUTINFO));
  plii->dwTime = mock_idle_time;
  return TRUE;
}

// TestIdle task fires after 100ms of idle time.
class TestIdleTask : public IdleTimerTask {
 public:
  TestIdleTask(bool repeat)
    : IdleTimerTask(TimeDelta::FromMilliseconds(100), repeat),
     idle_counter_(0) {
     set_last_input_info_fn(MockGetLastInputInfoFunction);
  }

  int get_idle_counter() { return idle_counter_; }

  virtual void OnIdle() {
    idle_counter_++;
  }

 private:
  int idle_counter_;
};

// A task to help us quit the test.
class TestFinishedTask : public Task {
 public:
  TestFinishedTask() {}
  void Run() {
    MessageLoop::current()->Quit();
  }
};

// A timer which resets the idle clock.
class ResetIdleTask : public Task {
 public:
  ResetIdleTask() {}
  void Run() {
    mock_idle_time = GetTickCount();
  }
};

///////////////////////////////////////////////////////////////////////////////
// NoRepeat tests:
// A non-repeating idle timer will fire once on idle, and
// then will not fire again unless it goes non-idle first.

TEST(IdleTimerTest, NoRepeatIdle) {
  // Create an IdleTimer, which should fire once after 100ms.
  // Create a Quit timer which will fire after 1s.
  // Verify that we fired exactly once.

  mock_idle_time = GetTickCount();
  TestIdleTask test_task(false);
  TestFinishedTask finish_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t = loop->timer_manager()->StartTimer(1000, &finish_task, false);
  test_task.Start();
  loop->Run();

  EXPECT_EQ(test_task.get_idle_counter(), 1);
  delete t;
}

TEST(IdleTimerTest, NoRepeatFlipIdleOnce) {
  // Create an IdleTimer, which should fire once after 100ms.
  // Create a Quit timer which will fire after 1s.
  // Create a timer to reset once, idle after 500ms.
  // Verify that we fired exactly twice.

  mock_idle_time = GetTickCount();
  TestIdleTask test_task(false);
  TestFinishedTask finish_task;
  ResetIdleTask reset_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t1 = loop->timer_manager()->StartTimer(1000, &finish_task, false);
  Timer* t2 = loop->timer_manager()->StartTimer(500, &reset_task, false);
  test_task.Start();
  loop->Run();

  EXPECT_EQ(test_task.get_idle_counter(), 2);
  delete t1;
  delete t2;
}

TEST(IdleTimerTest, NoRepeatNotIdle) {
  // Create an IdleTimer, which should fire once after 100ms.
  // Create a Quit timer which will fire after 1s.
  // Create a timer to reset idle every 50ms.
  // Verify that we never fired.

  mock_idle_time = GetTickCount();
  TestIdleTask test_task(false);
  TestFinishedTask finish_task;
  ResetIdleTask reset_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t = loop->timer_manager()->StartTimer(1000, &finish_task, false);
  Timer* reset_timer = loop->timer_manager()->StartTimer(50, &reset_task, true);
  test_task.Start();
  loop->Run();
  loop->timer_manager()->StopTimer(reset_timer);

  EXPECT_EQ(test_task.get_idle_counter(), 0);
  delete t;
  delete reset_timer;
}

///////////////////////////////////////////////////////////////////////////////
// Repeat tests:
// A repeating idle timer will fire repeatedly on each interval, as long
// as it has been idle.  So, if the machine remains idle, it will continue
// firing over and over.

TEST(IdleTimerTest, Repeat) {
  // Create an IdleTimer, which should fire repeatedly after 100ms.
  // Create a Quit timer which will fire after 1.05s.
  // Verify that we fired 10 times.
  mock_idle_time = GetTickCount();
  TestIdleTask test_task(true);
  TestFinishedTask finish_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t = loop->timer_manager()->StartTimer(1050, &finish_task, false);
  test_task.Start();
  loop->Run();

  // In a perfect world, the idle_counter should be 10.  However,
  // since timers aren't guaranteed to fire perfectly, this can
  // be less.  Just expect more than 5 and no more than 10.
  EXPECT_GT(test_task.get_idle_counter(), 5);
  EXPECT_LE(test_task.get_idle_counter(), 10);
  delete t;
}

TEST(IdleTimerTest, RepeatIdleReset) {
  // Create an IdleTimer, which should fire repeatedly after 100ms.
  // Create a Quit timer which will fire after 1s.
  // Create a reset timer, which fires after 550ms
  // Verify that we fired 9 times.
  mock_idle_time = GetTickCount();
  TestIdleTask test_task(true);
  ResetIdleTask reset_task;
  TestFinishedTask finish_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t1 = loop->timer_manager()->StartTimer(1000, &finish_task, false);
  Timer* t2 = loop->timer_manager()->StartTimer(550, &reset_task, false);
  test_task.Start();
  loop->Run();

  // In a perfect world, the idle_counter should be 9.  However,
  // since timers aren't guaranteed to fire perfectly, this can
  // be less.  Just expect more than 5 and no more than 9.
  EXPECT_GT(test_task.get_idle_counter(), 5);
  EXPECT_LE(test_task.get_idle_counter(), 9);
  delete t1;
  delete t2;
}

TEST(IdleTimerTest, RepeatNotIdle) {
  // Create an IdleTimer, which should fire repeatedly after 100ms.
  // Create a Quit timer which will fire after 1s.
  // Create a timer to reset idle every 50ms.
  // Verify that we never fired.

  mock_idle_time = GetTickCount();
  TestIdleTask test_task(true);
  TestFinishedTask finish_task;
  ResetIdleTask reset_task;
  MessageLoop* loop = MessageLoop::current();
  Timer* t1 = loop->timer_manager()->StartTimer(1000, &finish_task, false);
  Timer* reset_timer = loop->timer_manager()->StartTimer(50, &reset_task, true);
  test_task.Start();
  loop->Run();
  loop->timer_manager()->StopTimer(reset_timer);

  EXPECT_EQ(test_task.get_idle_counter(), 0);
  delete t1;
  delete reset_timer;
}
