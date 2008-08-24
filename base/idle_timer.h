// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IDLE_TIMER_H__
#define BASE_IDLE_TIMER_H__

#include <windows.h>

#include "base/basictypes.h"
#include "base/task.h"
#include "base/timer.h"

// IdleTimer is a recurring Timer task which runs only when the system is idle.
// System Idle time is defined as not having any user keyboard or mouse
// activity for some period of time.  Because the timer is user dependant, it
// is possible for the timer to never fire.

//
// Usage should be for low-priority tasks, and may look like this:
//
//   class MyIdleTimerTask : public IdleTimerTask {
//    public:
//     // This task will run after 5 seconds of idle time
//     // and not more often than once per minute.
//     MyIdleTimerTask() : IdleTimerTask(5, 60) {};
//     virtual void OnIdle() { do something };
//   }
//
//   MyIdleTimerTask *task = new MyIdleTimerTask();
//   task->Start();
//
//   // As with all TimerTasks, the caller must dispose the object.
//   delete task;  // Will Stop the timer and cleanup the task.

class TimerManager;

// Function prototype for GetLastInputInfo.
typedef BOOL (__stdcall *GetLastInputInfoFunction)(PLASTINPUTINFO plii);

class IdleTimerTask : public Task {
 public:
  // Create an IdleTimerTask.
  // idle_time: idle time required before this task can run.
  // repeat: true if the timer should fire multiple times per idle,
  //         false to fire once per idle.
  IdleTimerTask(TimeDelta idle_time, bool repeat);

  // On destruction, the IdleTimerTask will Stop itself and delete the Task.
  virtual ~IdleTimerTask();

  // Start the IdleTimerTask.
  void Start();

  // Stop the IdleTimertask.
  void Stop();

  // The method to run when the timer elapses.
  virtual void OnIdle() = 0;

 protected:
  // Override the GetLastInputInfo function.
  void set_last_input_info_fn(GetLastInputInfoFunction function) {
   get_last_input_info_fn_ = function;
  }

 private:
  // This task's run method.
  virtual void Run();

  // Start the timer.
  void StartTimer();

  // Gets the number of milliseconds since the last input event.
  TimeDelta CurrentIdleTime();

  // Compute time until idle.  Returns 0 if we are now idle.
  TimeDelta TimeUntilIdle();

  TimeDelta idle_interval_;
  bool repeat_;
  Time last_time_fired_;  // The last time the idle timer fired.
                          // will be 0 until the timer fires the first time.
  scoped_ptr<OneShotTimer> timer_;

  GetLastInputInfoFunction get_last_input_info_fn_;
};

#endif  // BASE_IDLE_TIMER_H__

