// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

class OneShotTimerTester {
 public:
  OneShotTimerTester(bool* did_run) : did_run_(did_run) {
  }
  void Start() {
    timer_.Start(TimeDelta::FromMilliseconds(10), this,
                 &OneShotTimerTester::Run);
  }
 private:
  void Run() {
    *did_run_ = true;
    MessageLoop::current()->Quit();
  }
  bool* did_run_;
  base::OneShotTimer<OneShotTimerTester> timer_;
};

class OneShotSelfDeletingTimerTester {
 public:
  OneShotSelfDeletingTimerTester(bool* did_run) :
      did_run_(did_run),
      timer_(new base::OneShotTimer<OneShotSelfDeletingTimerTester>()) {
  }
  void Start() {
    timer_->Start(TimeDelta::FromMilliseconds(10), this,
                  &OneShotSelfDeletingTimerTester::Run);
  }
 private:
  void Run() {
    *did_run_ = true;
    timer_.reset();
    MessageLoop::current()->Quit();
  }
  bool* did_run_;
  scoped_ptr<base::OneShotTimer<OneShotSelfDeletingTimerTester> > timer_;
};

class RepeatingTimerTester {
 public:
  RepeatingTimerTester(bool* did_run) : did_run_(did_run), counter_(10) {
  }
  void Start() {
    timer_.Start(TimeDelta::FromMilliseconds(10), this,
                 &RepeatingTimerTester::Run);
  }
 private:
  void Run() {
    if (--counter_ == 0) {
      *did_run_ = true;
      MessageLoop::current()->Quit();
    }
  }
  bool* did_run_;
  int counter_;
  base::RepeatingTimer<RepeatingTimerTester> timer_;
};

void RunTest_OneShotTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_OneShotTimer_Cancel(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  OneShotTimerTester* a = new OneShotTimerTester(&did_run_a);

  // This should run before the timer expires.
  MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();
 
  bool did_run_b = false;
  OneShotTimerTester b(&did_run_b);
  b.Start();

  MessageLoop::current()->Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

void RunTest_OneShotSelfDeletingTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotSelfDeletingTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  RepeatingTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingTimer_Cancel(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  RepeatingTimerTester* a = new RepeatingTimerTester(&did_run_a);

  // This should run before the timer expires.
  MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();
 
  bool did_run_b = false;
  RepeatingTimerTester b(&did_run_b);
  b.Start();

  MessageLoop::current()->Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that timers work properly in all configurations.

TEST(TimerTest, OneShotTimer) {
  RunTest_OneShotTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotTimer(MessageLoop::TYPE_UI);
  RunTest_OneShotTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, OneShotTimer_Cancel) {
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_UI);
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_IO);
}

// If underline timer does not handle properly, we will crash or fail
// in full page heap or purify environment.
TEST(TimerTest, OneShotSelfDeletingTimer) {
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_UI);
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, RepeatingTimer) {
  RunTest_RepeatingTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_RepeatingTimer(MessageLoop::TYPE_UI);
  RunTest_RepeatingTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, RepeatingTimer_Cancel) {
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_DEFAULT);
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_UI);
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_IO);
}

TEST(TimerTest, MessageLoopShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking (or purify) is enabled.
  bool did_run = false;
  {
    OneShotTimerTester a(&did_run);
    OneShotTimerTester b(&did_run);
    OneShotTimerTester c(&did_run);
    OneShotTimerTester d(&did_run);
    {
      MessageLoop loop(MessageLoop::TYPE_DEFAULT);
      a.Start();
      b.Start();
    }  // MessageLoop destructs by falling out of scope.
  }  // OneShotTimers destruct.  SHOULD NOT CRASH, of course.

  EXPECT_EQ(false, did_run);
}
