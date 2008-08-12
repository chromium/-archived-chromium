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

#ifndef BASE_TIMER_H_
#define BASE_TIMER_H_

#include <math.h>

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

// Timer/TimerManager are objects designed to help setting timers.
// Goals of TimerManager:
// - have only one Windows system timer for all app timer functionality
// - work around bugs with timers firing arbitrarily earlier than specified
// - provide the ability to run timers even if the application is in a
//   windows modal app loop.

class TimerManager;
class Task;
class MessageLoop;

class Timer {
 public:
  Timer(int delay, Task* task, bool repeating);

  Task* task() const { return task_; }
  void set_task(Task* task) { task_ = task; }

  int current_delay() const {
    // Be careful here.  Timers have a precision of microseconds,
    // but this API is in milliseconds.  If there are 5.5ms left,
    // should the delay be 5 or 6?  It should be 6 to avoid timers
    // firing early.  Implement ceiling by adding 999us prior to
    // conversion to ms.
    double delay = ceil((fire_time_ - Time::Now()).InMillisecondsF());
    return static_cast<int>(delay);
  }

  const Time &fire_time() const { return fire_time_; }

  bool repeating() const { return repeating_; }

  // Update (or fill in) creation_time_, and calculate future fire_time_ based
  // on current time plus delay_.
  void Reset();

  int id() const { return timer_id_; }

 protected:
  // Protected (rather than private) so that we can access from unit tests.

  // The time when the timer should fire.
  Time fire_time_;

 private:
  // The task that is run when this timer fires.
  Task* task_;

  // Timer delay in milliseconds.
  int delay_;

  // A monotonically increasing timer id.  Used
  // for ordering two timers which have the same
  // timestamp in a FIFO manner.
  int timer_id_;

  // Whether or not this timer repeats.
  const bool repeating_;

  // The tick count when the timer was "created". (i.e. when its current
  // iteration started.)
  Time creation_time_;

  DISALLOW_EVIL_CONSTRUCTORS(Timer);
};

class TimerComparison {
 public:
  bool operator() (const Timer* t1, const Timer* t2) const {
    const Time& f1 = t1->fire_time();
    const Time& f2 = t2->fire_time();
    // If the two timers have the same delay, revert to using
    // the timer_id to maintain FIFO ordering.
    if (f1 == f2) {
      // Gracefully handle wrap as we try to return (t1->id() > t2->id());
      int delta = t1->id() - t2->id();
      // Assuming the delta is smaller than 2**31, we'll always get the right
      // answer (in terms of sign of delta).
      return delta > 0;
    }
    return f1 > f2;
  }
};

// Subclass priority_queue to provide convenient access to removal from this
// list.
//
// Terminology: The "pending" timer is the timer at the top of the queue,
//              i.e. the timer whose task needs to be Run next.
class TimerPQueue : public std::priority_queue<Timer*,
                                               std::vector<Timer*>,
                                               TimerComparison> {
 public:
  // Removes |timer| from the queue.
  void RemoveTimer(Timer* timer);

  // Returns true if the queue contains |timer|.
  bool ContainsTimer(const Timer* timer) const;
};

// There is one TimerManager per thread, owned by the MessageLoop.
// Timers can either be fired directly by the MessageLoop, or by
// SetTimer and a WM_TIMER message.  The advantage of the former
// is that we can make timers fire significantly faster than the 10ms
// granularity provided by SetTimer().  The advantage of SetTimer()
// is that modal message loops which don't run our MessageLoop
// code will still be able to process WM_TIMER messages.
//
// Note:  TimerManager is not thread safe.  You cannot set timers
//        onto a thread other than your own.
class TimerManager {
 public:
  TimerManager();
  ~TimerManager();

  // Create and start a new timer. |task| is owned by the caller, as is the
  // timer object that is returned.
  Timer* StartTimer(int delay, Task* task, bool repeating);

  // Flag indicating whether the timer manager should use the OS
  // timers or not.  Default is true.  MessageLoops which are not reliably
  // called due to nested windows message loops should set this to
  // true.
  bool use_native_timers() { return use_native_timers_; }
  void set_use_native_timers(bool value) { use_native_timers_ = value; }

  // Starts a timer.  This is a no-op if the timer is already started.
  void StartTimer(Timer* timer);

  // Stop a timer.  This is a no-op if the timer is already stopped.
  void StopTimer(Timer* timer);

  // Reset an existing timer, which may or may not be currently in the queue of
  // upcoming timers.  The timer's parameters are unchanged; it simply begins
  // counting down again as if it was just created.
  void ResetTimer(Timer* timer);

  // Returns true if |timer| is in the queue of upcoming timers.
  bool IsTimerRunning(const Timer* timer) const;

  // Run some small number of timers.
  // Returns true if it runs a task, false otherwise.
  bool RunSomePendingTimers();

  // The number of milliseconds remaining until the pending timer (top of the
  // pqueue) needs to be fired. Returns -1 if no timers are pending.
  int GetCurrentDelay();

  // A handler for WM_TIMER messages.
  // If a task is not running currently, it runs some timer tasks (if there are
  // some ready to fire), otherwise it just updates the WM_TIMER to be called
  // again (hopefully when it is allowed to run a task).
  int MessageWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Return cached copy of MessageLoop::current().
  MessageLoop* message_loop();

#ifdef UNIT_TEST
  // For testing only, used to simulate broken early-firing WM_TIMER
  // notifications by setting arbitrarily small delays in SetTimer.
  void set_use_broken_delay(bool use_broken_delay) {
    use_broken_delay_ = use_broken_delay;
  }
#endif  // UNIT_TEST

 protected:
  // Peek at the timer which will fire soonest.
  Timer* PeekTopTimer();

 private:
  // Update our Windows WM_TIMER to match our most immediately pending timer.
  void UpdateWindowsWmTimer();

#ifdef OS_WIN
  // Retrieve the Message Window that handles WM_TIMER messages from the
  // system.
  HWND GetMessageHWND();

  HWND message_hwnd_;
#endif  // OS_WIN

  TimerPQueue timers_;

  bool use_broken_delay_;

  // Flag to enable/disable use of native timers.
  bool use_native_timers_;

  // A lazily cached copy of MessageLoop::current.
  MessageLoop* message_loop_;

  DISALLOW_EVIL_CONSTRUCTORS(TimerManager);
};

// A simple wrapper for the Timer / TimerManager API.  This is a helper class.
// Use OneShotTimer or RepeatingTimer instead.
class SimpleTimer {
 public:
  // Stops the timer.
  ~SimpleTimer();

  // Call this method to explicitly start the timer.  This is a no-op if the
  // timer is already running.
  void Start();

  // Call this method to explicitly stop the timer.  This is a no-op if the
  // timer is not running.
  void Stop();

  // Returns true if the timer is running (i.e., not stopped).
  bool IsRunning() const;

  // Short-hand for calling Stop and then Start.
  void Reset();

  // Get/Set the task to be run when this timer expires.  NOTE: The caller of
  // set_task must be careful to ensure that the old task is properly deleted.
  Task* task() const { return timer_.task(); }
  void set_task(Task* task) {
    timer_.set_task(task);
    owns_task_ = true;
  }

  // Sets the task, but marks it so it shouldn't be deleted by the SimpleTimer.
  void set_unowned_task(Task* task) {
    timer_.set_task(task);
    owns_task_ = false;
  }

 protected:
  SimpleTimer(TimeDelta delay, Task* task, bool repeating);

 private:
  Timer timer_;

  // Whether we need to clean up the Task* object for this Timer when
  // we are deallocated. Defaults to true.
  bool owns_task_;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleTimer);
};

// A simple, one-shot timer.  The task is run after the specified delay once
// the Start method is called.  The task is deleted when the timer object is
// destroyed.
class OneShotTimer : public SimpleTimer {
 public:
  // The task must be set using set_task before calling Start.
  explicit OneShotTimer(TimeDelta delay)
      : SimpleTimer(delay, NULL, false) {
  }
  // If task is null, then it must be set using set_task before calling Start.
  OneShotTimer(TimeDelta delay, Task* task)
      : SimpleTimer(delay, task, false) {
  }
 private:
  DISALLOW_EVIL_CONSTRUCTORS(OneShotTimer);
};

// A simple, repeating timer.  The task is run at the specified interval once
// the Start method is called.  The task is deleted when the timer object is
// destroyed.
class RepeatingTimer : public SimpleTimer {
 public:
  // The task must be set using set_task before calling Start.
  explicit RepeatingTimer(TimeDelta interval)
      : SimpleTimer(interval, NULL, true) {
  }
  // If task is null, then it must be set using set_task before calling Start.
  RepeatingTimer(TimeDelta interval, Task* task)
      : SimpleTimer(interval, task, true) {
  }
 private:
  DISALLOW_EVIL_CONSTRUCTORS(RepeatingTimer);
};

#endif  // BASE_TIMER_H_
