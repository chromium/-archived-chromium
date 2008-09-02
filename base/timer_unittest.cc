// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/task.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimerComparison;

namespace {

class TimerTest : public testing::Test {};

// A base class timer task that sanity-checks timer functionality and counts
// the number of times it has run.  Handles all message loop and memory
// management issues.
class TimerTask : public Task {
 public:
  // Runs all timers to completion.  This returns only after all timers have
  // finished firing.
  static void RunTimers();

  // Creates a new timer.  If |repeating| is true, the timer will repeat 10
  // times before terminating.
  //
  // All timers are managed on the message loop of the thread that calls this
  // function the first time.
  TimerTask(int delay, bool repeating);

  virtual ~TimerTask();

  int iterations() const { return iterations_; }
  const Timer* timer() const { return timer_; }

  // Resets the timer, if it exists.
  void Reset();

  // Task
  virtual void Run();

 protected:
  // Shuts down the message loop if necessary.
  static void QuitMessageLoop();

 private:
  static MessageLoop* message_loop() {
    return MessageLoop::current();
  }

  static int timer_count_;
  static bool loop_running_;

  bool timer_running_;
  int delay_;
  TimeTicks start_ticks_;
  int iterations_;
  Timer* timer_;
};

// static
void TimerTask::RunTimers() {
  if (timer_count_ && !loop_running_) {
    loop_running_ = true;
    message_loop()->Run();
  }
}

TimerTask::TimerTask(int delay, bool repeating)
    : timer_running_(false),
      delay_(delay),
      start_ticks_(TimeTicks::Now()),
      iterations_(0),
      timer_(NULL) {
  Reset();  // This will just set up the variables to indicate we have a
            // running timer.
  timer_ = message_loop()->timer_manager()->StartTimer(delay, this, repeating);
}

TimerTask::~TimerTask() {
  if (timer_) {
    message_loop()->timer_manager()->StopTimer(timer_);
    delete timer_;
  }
  if (timer_running_) {
    timer_running_ = false;
    if (--timer_count_ <= 0)
      QuitMessageLoop();
  }
}

void TimerTask::Reset() {
  if (!timer_running_) {
    timer_running_ = true;
    ++timer_count_;
  }
  if (timer_) {
    start_ticks_ = TimeTicks::Now();
    message_loop()->timer_manager()->ResetTimer(timer_);
  }
}

void TimerTask::Run() {
  ++iterations_;

  // Test that we fired on or after the delay, not before.
  const TimeTicks ticks = TimeTicks::Now();
  EXPECT_LE(delay_, (ticks - start_ticks_).InMilliseconds());
  // Note: Add the delay rather than using the ticks recorded.
  //       Repeating timers have already started ticking before
  //       this callback; we pretend they started *now*, then
  //       it might seem like they fire early, when they do not.
  start_ticks_ += TimeDelta::FromMilliseconds(delay_);

  // If we're done running, shut down the message loop.
  if (timer_->repeating() && (iterations_ < 10))
    return;  // Iterate 10 times before terminating.
  message_loop()->timer_manager()->StopTimer(timer_);
  timer_running_ = false;
  if (--timer_count_ <= 0)
    QuitMessageLoop();
}

// static
void TimerTask::QuitMessageLoop() {
  if (loop_running_) {
    message_loop()->Quit();
    loop_running_ = false;
  }
}

int TimerTask::timer_count_ = 0;
bool TimerTask::loop_running_ = false;

// A task that deletes itself when run.
class DeletingTask : public TimerTask {
 public:
  DeletingTask(int delay, bool repeating) : TimerTask(delay, repeating) { }

  // Task
  virtual void Run();
};

void DeletingTask::Run() {
  delete this;

  // Can't call TimerTask::Run() here, we've destroyed ourselves.
}

// A class that resets another TimerTask when run.
class ResettingTask : public TimerTask {
 public:
  ResettingTask(int delay, bool repeating, TimerTask* task)
      : TimerTask(delay, repeating),
        task_(task) {
  }

  virtual void Run();

 private:
  TimerTask* task_;
};

void ResettingTask::Run() {
  task_->Reset();

  TimerTask::Run();
}

// A class that quits the message loop when run.
class QuittingTask : public TimerTask {
 public:
  QuittingTask(int delay, bool repeating) : TimerTask(delay, repeating) { }

  virtual void Run();
};

void QuittingTask::Run() {
  QuitMessageLoop();

  TimerTask::Run();
}

void RunTimerTest() {
  // Make sure oneshot timers work correctly.
  TimerTask task1(100, false);
  TimerTask::RunTimers();
  EXPECT_EQ(1, task1.iterations());

  // Make sure repeating timers work correctly.
  TimerTask task2(10, true);
  TimerTask task3(100, true);
  TimerTask::RunTimers();
  EXPECT_EQ(10, task2.iterations());
  EXPECT_EQ(10, task3.iterations());
}

//-----------------------------------------------------------------------------
// The timer test cases:

void RunTest_TimerComparison(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Make sure TimerComparison sorts correctly.
  const TimerTask task1(10, false);
  const Timer* timer1 = task1.timer();
  const TimerTask task2(200, false);
  const Timer* timer2 = task2.timer();
  TimerComparison comparison;
  EXPECT_FALSE(comparison(timer1, timer2));
  EXPECT_TRUE(comparison(timer2, timer1));
}

void RunTest_BasicTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  RunTimerTest();
}

void RunTest_BrokenTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Simulate faulty early-firing timers. The tasks in RunTimerTest should
  // nevertheless be invoked after their specified delays, regardless of when
  // WM_TIMER fires.
  TimerManager* manager = MessageLoop::current()->timer_manager();
  manager->set_use_broken_delay(true);
  RunTimerTest();
  manager->set_use_broken_delay(false);
}

void RunTest_DeleteFromRun(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Make sure TimerManager correctly handles a Task that deletes itself when
  // run.
  new DeletingTask(50, true);
  TimerTask timer_task(150, false);
  new DeletingTask(250, true);
  TimerTask::RunTimers();
  EXPECT_EQ(1, timer_task.iterations());
}

void RunTest_Reset(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Make sure resetting a timer after it has fired works.
  TimerTask timer_task1(250, false);
  TimerTask timer_task2(100, true);
  ResettingTask resetting_task1(600, false, &timer_task1);
  TimerTask::RunTimers();
  EXPECT_EQ(2, timer_task1.iterations());
  EXPECT_EQ(10, timer_task2.iterations());

  // Make sure resetting a timer before it has fired works.  This will reset
  // two timers, then stop the message loop between when they should have
  // finally fired.
  TimerTask timer_task3(100, false);
  TimerTask timer_task4(600, false);
  ResettingTask resetting_task3(50, false, &timer_task3);
  ResettingTask resetting_task4(50, false, &timer_task4);
  QuittingTask quitting_task(300, false);
  TimerTask::RunTimers();
  EXPECT_EQ(1, timer_task3.iterations());
  EXPECT_EQ(0, timer_task4.iterations());
}

void RunTest_FifoOrder(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Creating timers with the same timeout should
  // always compare to result in FIFO ordering.

  // Derive from the timer so that we can set it's fire time.
  // We have to do this, because otherwise, it's possible for
  // two timers, created back to back, to have different times,
  // and in that case, we aren't really testing what we want
  // to test!
  class MockTimer : public Timer {
   public:
    MockTimer(int delay) : Timer(delay, NULL, false) {}
    void set_fire_time(const Time& t) { fire_time_ = t; }
  };

  class MockTimerManager : public TimerManager {
   public:
    MockTimerManager() : TimerManager(MessageLoop::current()) {
    }
    
    // Pops the most-recent to fire timer and returns its timer id.
    // Returns -1 if there are no timers in the list.
    int pop() {
      int rv = -1;
      Timer* top = PeekTopTimer();
      if (top) {
        rv = top->id();
        StopTimer(top);
        delete top;
      }
      return rv;
    }
  };

  MockTimer t1(0);
  MockTimer t2(0);
  t2.set_fire_time(t1.fire_time());
  TimerComparison comparison;
  EXPECT_TRUE(comparison(&t2, &t1));

  // Issue a tight loop of timers; most will have the
  // same timestamp; some will not.  Either way, since
  // all are created with delay(0), the second timer
  // must always be greater than the first.  Then, pop
  // all the timers and verify that it's a FIFO list.
  MockTimerManager manager;
  const int kNumTimers = 1024;
  for (int i=0; i < kNumTimers; i++)
    manager.StartTimer(0, NULL, false);

  int last_id = -1;
  int new_id = 0;
  while((new_id = manager.pop()) > 0)
    EXPECT_GT(new_id, last_id);
}

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

}  // namespace

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

TEST(TimerTest, TimerComparison) {
  Time s = Time::Now();
  RunTest_TimerComparison(MessageLoop::TYPE_DEFAULT);
  RunTest_TimerComparison(MessageLoop::TYPE_UI);
  RunTest_TimerComparison(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("comparison elapsed time %lld\n", el.ToInternalValue());
}

TEST(TimerTest, BasicTimer) {
  Time s = Time::Now();
  RunTest_BasicTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicTimer(MessageLoop::TYPE_UI);
  RunTest_BasicTimer(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("basic elapsed time %lld\n", el.ToInternalValue());
}

TEST(TimerTest, BrokenTimer) {
  Time s = Time::Now();
  RunTest_BrokenTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_BrokenTimer(MessageLoop::TYPE_UI);
  RunTest_BrokenTimer(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("broken elapsed time %lld\n", el.ToInternalValue());
}

TEST(TimerTest, DeleteFromRun) {
  Time s = Time::Now();
  RunTest_DeleteFromRun(MessageLoop::TYPE_DEFAULT);
  RunTest_DeleteFromRun(MessageLoop::TYPE_UI);
  RunTest_DeleteFromRun(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("delete elapsed time %lld\n", el.ToInternalValue());
}

TEST(TimerTest, Reset) {
  Time s = Time::Now();
  RunTest_Reset(MessageLoop::TYPE_DEFAULT);
  RunTest_Reset(MessageLoop::TYPE_UI);
  RunTest_Reset(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("reset elapsed time %lld\n", el.ToInternalValue());
}

TEST(TimerTest, FifoOrder) {
  Time s = Time::Now();
  RunTest_FifoOrder(MessageLoop::TYPE_DEFAULT);
  RunTest_FifoOrder(MessageLoop::TYPE_UI);
  RunTest_FifoOrder(MessageLoop::TYPE_IO);
  Time e = Time::Now();
  TimeDelta el = e - s;
  printf("fifo elapsed time %lld\n", el.ToInternalValue());
}

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
