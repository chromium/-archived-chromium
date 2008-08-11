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
