// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/idle_timer.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::IdleTimer;

namespace {

class IdleTimerTest : public testing::Test {
 private:
  // IdleTimer requires a UI message loop on the current thread.
  MessageLoopForUI message_loop_;
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
class TestIdleTask : public IdleTimer {
 public:
  TestIdleTask(bool repeat)
      : IdleTimer(TimeDelta::FromMilliseconds(100), repeat),
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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// NoRepeat tests:
// A non-repeating idle timer will fire once on idle, and
// then will not fire again unless it goes non-idle first.

TEST_F(IdleTimerTest, NoRepeatIdle) {
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

TEST_F(IdleTimerTest, NoRepeatFlipIdleOnce) {
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

TEST_F(IdleTimerTest, NoRepeatNotIdle) {
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

TEST_F(IdleTimerTest, Repeat) {
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

TEST_F(IdleTimerTest, RepeatIdleReset) {
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

TEST_F(IdleTimerTest, RepeatNotIdle) {
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

