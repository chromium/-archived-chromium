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

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"

//-----------------------------------------------------------------------------
// Timer/TimerManager are objects designed to help setting timers.
// Goals of TimerManager:
// - have only one system timer for all app timer functionality
// - work around bugs with timers firing arbitrarily earlier than specified
// - provide the ability to run timers even if the application is in a
//   windows modal app loop.
//-----------------------------------------------------------------------------

class MessageLoop;
class TimerManager;
class Task;

//-----------------------------------------------------------------------------
// The core timer object.  Use TimerManager to create and control timers.
class Timer {
 public:
  Timer(int delay, Task* task, bool repeating);

  // The task to be run when the timer fires.
  Task* task() const { return task_; }
  void set_task(Task* task) { task_ = task; }

  // Returns the absolute time at which the timer should fire.
  const Time &fire_time() const { return fire_time_; }

  // A repeating timer is a timer that is automatically scheduled to fire again
  // after it fires.
  bool repeating() const { return repeating_; }

  // Update (or fill in) creation_time_, and calculate future fire_time_ based
  // on current time plus delay_.
  void Reset();

  // A unique identifier for this timer.
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

  // A monotonically increasing timer id.  Used for ordering two timers which
  // have the same timestamp in a FIFO manner.
  int timer_id_;

  // Whether or not this timer repeats.
  const bool repeating_;

  // The tick count when the timer was "created". (i.e. when its current
  // iteration started.)
  Time creation_time_;

  DISALLOW_COPY_AND_ASSIGN(Timer);
};

//-----------------------------------------------------------------------------
// Used to implement TimerPQueue
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

//-----------------------------------------------------------------------------
// Subclass priority_queue to provide convenient access to removal from this
// list.
//
// Terminology: The "pending" timer is the timer at the top of the queue,
//              i.e. the timer whose task needs to be Run next.
class TimerPQueue :
    public std::priority_queue<Timer*, std::vector<Timer*>, TimerComparison> {
 public:
  // Removes |timer| from the queue.
  void RemoveTimer(Timer* timer);

  // Returns true if the queue contains |timer|.
  bool ContainsTimer(const Timer* timer) const;
};

//-----------------------------------------------------------------------------
// There is one TimerManager per thread, owned by the MessageLoop.  Timers can
// either be fired by the MessageLoop from within its run loop or via a system
// timer event that the MesssageLoop constructs.  The advantage of the former
// is that we can make timers fire significantly faster than the granularity
// provided by the system.  The advantage of a system timer is that modal
// message loops which don't run our MessageLoop code will still be able to
// process system timer events.
//
// NOTE:  TimerManager is not thread safe.  You cannot set timers onto a thread
//        other than your own.
class TimerManager {
 public:
  explicit TimerManager(MessageLoop* message_loop);
  ~TimerManager();

  // Create and start a new timer. |task| is owned by the caller, as is the
  // timer object that is returned.
  Timer* StartTimer(int delay, Task* task, bool repeating);

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

  // The absolute time at which the next timer is to fire.  If there is not a
  // next timer to run, then the is_null property of the returned Time object
  // will be true.  NOTE: This could be a time in the past!
  Time GetNextFireTime() const;

#ifdef UNIT_TEST
  // For testing only, used to simulate broken early-firing WM_TIMER
  // notifications by setting arbitrarily small delays in SetTimer.
  void set_use_broken_delay(bool use_broken_delay) {
    use_broken_delay_ = use_broken_delay;
  }
#endif  // UNIT_TEST

  bool use_broken_delay() const {
    return use_broken_delay_;
  }

 protected:
  // Peek at the timer which will fire soonest.
  Timer* PeekTopTimer();

 private:
  void DidChangeNextTimer();

  // A cached value that indicates the time when we think the next timer is to
  // fire.  We use this to determine if we should call DidChangeNextTimerExpiry
  // on the MessageLoop.
  Time next_timer_expiry_;

  TimerPQueue timers_;

  bool use_broken_delay_;

  // A lazily cached copy of MessageLoop::current.
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(TimerManager);
};

//-----------------------------------------------------------------------------
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

  DISALLOW_COPY_AND_ASSIGN(SimpleTimer);
};

//-----------------------------------------------------------------------------
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
  DISALLOW_COPY_AND_ASSIGN(OneShotTimer);
};

//-----------------------------------------------------------------------------
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
  DISALLOW_COPY_AND_ASSIGN(RepeatingTimer);
};

#endif  // BASE_TIMER_H_
