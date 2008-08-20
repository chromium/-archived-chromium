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

#include "base/message_pump_default.h"

#include "base/logging.h"
#include "base/time.h"

namespace base {

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(false, false) {
}

void MessagePumpDefault::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;
    if (did_work)
      continue;

    // TODO(darin): Delayed work will be starved if DoWork continues to return
    // true.  We should devise a better strategy.
    //
    // It is tempting to call DoWork followed by DoDelayedWork before checking
    // did_work, but we need to make sure that any tasks that were dispatched
    // prior to a timer actually run before the timer.  Getting that right may
    // require some additional changes.

    did_work = delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;
    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;
    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      event_.Wait();
    } else {
      TimeDelta delay = delayed_work_time_ - Time::Now();
      if (delay > TimeDelta()) {
        event_.TimedWait(delay);
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = Time();
      }
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }

  keep_running_ = true;
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base
