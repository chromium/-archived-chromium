// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OneShotTimer and RepeatingTimer provide a simple timer API.  As the names
// suggest, OneShotTimer calls you back once after a time delay expires.
// RepeatingTimer on the other hand calls you back periodically with the
// prescribed time interval.
//
// OneShotTimer and RepeatingTimer both cancel the timer when they go out of
// scope, which makes it easy to ensure that you do not get called when your
// object has gone out of scope.  Just instantiate a OneShotTimer or
// RepeatingTimer as a member variable of the class for which you wish to
// receive timer events.
//
// Sample RepeatingTimer usage:
//
//   class MyClass {
//    public:
//     void StartDoingStuff() {
//       timer_.Start(TimeDelta::FromSeconds(1), this, &MyClass::DoStuff);
//     }
//     void StopDoingStuff() {
//       timer_.Stop();
//     }
//    private:
//     void DoStuff() {
//       // This method is called every second to do stuff.
//       ...
//     }
//     base::RepeatingTimer<MyClass> timer_;
//   };
//
// Both OneShotTimer and RepeatingTimer also support a Reset method, which
// allows you to easily defer the timer event until the timer delay passes once
// again.  So, in the above example, if 0.5 seconds have already passed,
// calling Reset on timer_ would postpone DoStuff by another 1 second.  In
// other words, Reset is shorthand for calling Stop and then Start again with
// the same arguments.
//
// NOTE:  The older TimerManager / Timer API is deprecated.  New code should
// use OneShotTimer or RepeatingTimer.

#ifndef BASE_TIMER_H_
#define BASE_TIMER_H_

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
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

namespace base {

class TimerManager;

//-----------------------------------------------------------------------------
// The core timer object.  Use TimerManager to create and control timers.
//
// NOTE:  This class is DEPRECATED.  Do not use!
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
//
// NOTE:  This class is DEPRECATED.  Do not use!
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
//
// NOTE:  This class is DEPRECATED.  Do not use!
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
//
// NOTE:  This class is DEPRECATED.  Do not use!
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
// This class is an implementation detail of OneShotTimer and RepeatingTimer.
// Please do not use this class directly.
//
// This class exists to share code between BaseTimer<T> template instantiations.
//
class BaseTimer_Helper {
 public:
  // Stops the timer.
  ~BaseTimer_Helper() {
    OrphanDelayedTask();
  }

  // Returns true if the timer is running (i.e., not stopped).
  bool IsRunning() const {
    return delayed_task_ != NULL;
  }

 protected:
  BaseTimer_Helper(bool repeating)
      : delayed_task_(NULL), repeating_(repeating) {
  }

  // We have access to the timer_ member so we can orphan this task.
  class TimerTask : public Task {
   public:
    BaseTimer_Helper* timer_;
  };

  // Used to orphan delayed_task_ so that when it runs it does nothing.
  void OrphanDelayedTask();
  
  // Used to initiated a new delayed task.  This has the side-effect of
  // orphaning delayed_task_ if it is non-null.
  void InitiateDelayedTask(TimerTask* timer_task);

  TimerTask* delayed_task_;
  TimeDelta delay_;
  bool repeating_;

  DISALLOW_COPY_AND_ASSIGN(BaseTimer_Helper);
};

//-----------------------------------------------------------------------------
// This class is an implementation detail of OneShotTimer and RepeatingTimer.
// Please do not use this class directly.
template <class Receiver>
class BaseTimer : public BaseTimer_Helper {
 public:
  typedef void (Receiver::*ReceiverMethod)();

  // The task must be set using set_task before calling Start.
  BaseTimer(bool repeating)
      : BaseTimer_Helper(repeating), receiver_(NULL), receiver_method_(NULL) {
  }

  // Call this method to start the timer.  It is an error to call this method
  // while the timer is already running.
  void Start(TimeDelta delay, Receiver* receiver, ReceiverMethod method) {
    DCHECK(!IsRunning());
    delay_ = delay;
    receiver_ = receiver;
    receiver_method_ = method;
    InitiateDelayedTask(new TimerTask());
  }

  // Call this method to stop the timer.  It is a no-op if the timer is not
  // running.
  void Stop() {
    receiver_ = NULL;
    receiver_method_ = NULL;
    OrphanDelayedTask();
  }

  // Call this method to reset the timer delay of an already running timer.
  void Reset() {
    DCHECK(IsRunning());
    OrphanDelayedTask();
    InitiateDelayedTask(new TimerTask());
  }

 private:
  class TimerTask : public BaseTimer_Helper::TimerTask {
   public:
    virtual void Run() {
      if (!timer_)  // timer_ is null if we were orphaned.
        return;
      BaseTimer<Receiver>* self = static_cast<BaseTimer<Receiver>*>(timer_);
      self->delayed_task_ = NULL;
      if (self->repeating_)
        self->Reset();
      DispatchToMethod(self->receiver_, self->receiver_method_, Tuple0());
    }
  };
  
  Receiver* receiver_;
  ReceiverMethod receiver_method_;
};

//-----------------------------------------------------------------------------
// A simple, one-shot timer.  See usage notes at the top of the file.
template <class Receiver>
class OneShotTimer : public BaseTimer<Receiver> {
 public:
  OneShotTimer() : BaseTimer<Receiver>(false) {}
};

//-----------------------------------------------------------------------------
// A simple, repeating timer.  See usage notes at the top of the file.
template <class Receiver>
class RepeatingTimer : public BaseTimer<Receiver> {
 public:
  RepeatingTimer() : BaseTimer<Receiver>(true) {}
};

}  // namespace base

// TODO(darin): b/1346553: Remove these once Timer and TimerManager are unused.
using base::Timer;
using base::TimerManager;

#endif  // BASE_TIMER_H_
