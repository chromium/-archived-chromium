// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WAITABLE_EVENT_H_
#define BASE_WAITABLE_EVENT_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
typedef void* HANDLE;
#else
#include "base/condition_variable.h"
#include "base/lock.h"
#endif

class TimeDelta;

namespace base {

// A WaitableEvent can be a useful thread synchronization tool when you want to
// allow one thread to wait for another thread to finish some work.
//
// Use a WaitableEvent when you would otherwise use a Lock+ConditionVariable to
// protect a simple boolean value.  However, if you find yourself using a
// WaitableEvent in conjunction with a Lock to wait for a more complex state
// change (e.g., for an item to be added to a queue), then you should probably
// be using a ConditionVariable instead of a WaitableEvent.
//
// NOTE: On Windows, this class provides a subset of the functionality afforded
// by a Windows event object.  This is intentional.  If you are writing Windows
// specific code and you need other features of a Windows event, then you might
// be better off just using an Windows event directly.
//
class WaitableEvent {
 public:
  // If manual_reset is true, then to set the event state to non-signaled, a
  // consumer must call the Reset method.  If this parameter is false, then the
  // system automatically resets the event state to non-signaled after a single
  // waiting thread has been released.
  WaitableEvent(bool manual_reset, bool initially_signaled);

  // WARNING: Destroying a WaitableEvent while threads are waiting on it is not
  // supported.  Doing so will cause crashes or other instability.
  ~WaitableEvent();

  // Put the event in the un-signaled state.
  void Reset();

  // Put the event in the signaled state.  Causing any thread blocked on Wait
  // to be woken up.
  void Signal();

  // Returns true if the event is in the signaled state, else false.  If this
  // is not a manual reset event, then this test will cause a reset.
  bool IsSignaled();

  // Wait indefinitely for the event to be signaled.  Returns true if the event
  // was signaled, else false is returned to indicate that waiting failed.
  bool Wait();

  // Wait up until max_time has passed for the event to be signaled.  Returns
  // true if the event was signaled.  If this method returns false, then it
  // does not necessarily mean that max_time was exceeded.
  bool TimedWait(const TimeDelta& max_time);

 private:
#if defined(OS_WIN)
  HANDLE event_;
#else
  Lock lock_;  // Needs to be listed first so it will be constructed first.
  ConditionVariable cvar_;
  bool signaled_;
  bool manual_reset_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

}  // namespace base

#endif  // BASE_WAITABLE_EVENT_H_

