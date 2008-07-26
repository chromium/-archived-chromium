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

#include <windows.h>

#include "base/idle_timer.h"

#include "base/message_loop.h"
#include "base/time.h"

IdleTimerTask::IdleTimerTask(TimeDelta idle_time, bool repeat)
  : idle_interval_(idle_time),
    repeat_(repeat),
    get_last_input_info_fn_(GetLastInputInfo) {
}

IdleTimerTask::~IdleTimerTask() {
  Stop();
}

void IdleTimerTask::Start() {
  DCHECK(!timer_.get());
  StartTimer();
}

void IdleTimerTask::Stop() {
  timer_.reset();
}

void IdleTimerTask::Run() {
  // Verify we can fire the idle timer.
  if (TimeUntilIdle().InMilliseconds() <= 0) {
    OnIdle();
    last_time_fired_ = Time::Now();
  }
  Stop();
  StartTimer();  // Restart the timer for next run.
}

void IdleTimerTask::StartTimer() {
  DCHECK(timer_ == NULL);
  TimeDelta delay = TimeUntilIdle();
  if (delay.InMilliseconds() < 0)
    delay = TimeDelta();
  timer_.reset(new OneShotTimer(delay));
  timer_->set_unowned_task(this);
  timer_->Start();
}

TimeDelta IdleTimerTask::CurrentIdleTime() {
  // TODO(mbelshe): This is windows-specific code.
  LASTINPUTINFO info;
  info.cbSize = sizeof(info);
  if (get_last_input_info_fn_(&info)) {
    // Note: GetLastInputInfo returns a 32bit value which rolls over ~49days.
    int32 last_input_time = info.dwTime;
    int32 current_time = GetTickCount();
    int32 interval = current_time - last_input_time;
    // Interval will go negative if we've been idle for 2GB of ticks.
    if (interval < 0)
      interval = -interval;
    return TimeDelta::FromMilliseconds(interval);
  }
  NOTREACHED();
  return TimeDelta::FromMilliseconds(0);
}

TimeDelta IdleTimerTask::TimeUntilIdle() {
  TimeDelta time_since_last_fire = Time::Now() - last_time_fired_;
  TimeDelta current_idle_time = CurrentIdleTime();
  if (current_idle_time > time_since_last_fire) {
    if (repeat_)
      return idle_interval_ - time_since_last_fire;
    return idle_interval_;
  }
  return idle_interval_ - current_idle_time;
}
