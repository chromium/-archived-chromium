// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/waitable_event.h"

namespace base {

WaitableEvent::WaitableEvent(bool manual_reset, bool signaled)
    : lock_(),
      cvar_(&lock_),
      signaled_(signaled),
      manual_reset_(manual_reset) {
}

WaitableEvent::~WaitableEvent() {
  // Members are destroyed in the reverse of their initialization order, so we
  // should not have to worry about lock_ being destroyed before cvar_.
}

void WaitableEvent::Reset() {
  AutoLock locked(lock_);
  signaled_ = false;
}

void WaitableEvent::Signal() {
  AutoLock locked(lock_);
  if (!signaled_) {
    signaled_ = true;
    if (manual_reset_) {
      cvar_.Broadcast(); 
    } else {
      cvar_.Signal();
    }
  }
}

bool WaitableEvent::IsSignaled() {
  return TimedWait(TimeDelta::FromMilliseconds(0));
}

bool WaitableEvent::Wait() {
  AutoLock locked(lock_);
  while (!signaled_)
    cvar_.Wait();
  if (!manual_reset_)
    signaled_ = false;
  return true;
}

bool WaitableEvent::TimedWait(const TimeDelta& max_time) {
  AutoLock locked(lock_);
  // In case of spurious wake-ups, we need to adjust the amount of time that we
  // spend sleeping.
  TimeDelta total_time;
  for (;;) {
    TimeTicks start = TimeTicks::Now();
    cvar_.TimedWait(max_time - total_time);
    if (signaled_)
      break;
    total_time += TimeTicks::Now() - start;
    if (total_time >= max_time)
      break;
  }
  bool result = signaled_;
  if (!manual_reset_)
    signaled_ = false;
  return result;
}

}  // namespace base

