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

#include "base/watchdog.h"

#include "base/platform_thread.h"
#include "base/string_util.h"

//------------------------------------------------------------------------------
// Public API methods.

// Start thread running in a Disarmed state.
Watchdog::Watchdog(const TimeDelta& duration,
                   const std::wstring& thread_watched_name,
                   bool enabled)
  : lock_(),
    condition_variable_(&lock_),
    state_(DISARMED),
    duration_(duration),
    thread_watched_name_(thread_watched_name),
    handle_(NULL),
    thread_id_(0) {
  if (!enabled)
    return;  // Don't start thread, or doing anything really.
  handle_ = CreateThread(NULL,  // security
                         0,    // Default stack size.
                         Watchdog::ThreadStart,
                         reinterpret_cast<void*>(this),
                         CREATE_SUSPENDED,
                         &thread_id_);
  DCHECK(NULL != handle_);
  if (NULL == handle_)
    return ;
  ResumeThread(handle_);  // WINAPI call.
}

// Notify watchdog thread, and wait for it to finish up.
Watchdog::~Watchdog() {
  if (NULL == handle_)
    return;
  {
    AutoLock lock(lock_);
    state_ = SHUTDOWN;
  }
  condition_variable_.Signal();
  DWORD results = WaitForSingleObject(handle_, INFINITE);
  DCHECK(WAIT_OBJECT_0 == results);
  CloseHandle(handle_);
  handle_ = NULL;
}

void Watchdog::Arm() {
  ArmAtStartTime(TimeTicks::Now());
}

void Watchdog::ArmSomeTimeDeltaAgo(const TimeDelta& time_delta) {
  ArmAtStartTime(TimeTicks::Now() - time_delta);
}

// Start clock for watchdog.
void Watchdog::ArmAtStartTime(const TimeTicks start_time) {
  {
    AutoLock lock(lock_);
    start_time_ = start_time;
    state_ = ARMED;
  }
  // Force watchdog to wake up, and go to sleep with the timer ticking with the
  // proper duration.
  condition_variable_.Signal();
}

// Disable watchdog so that it won't do anything when time expires.
void Watchdog::Disarm() {
  if (NULL == handle_)
    return;
  AutoLock lock(lock_);
  state_ = DISARMED;
  // We don't need to signal, as the watchdog will eventually wake up, and it
  // will check its state and time, and act accordingly.
}

//------------------------------------------------------------------------------
// Internal private methods that the watchdog thread uses.

// static
DWORD __stdcall Watchdog::ThreadStart(void* pThis) {
  Watchdog* watchdog = reinterpret_cast<Watchdog*>(pThis);
  return watchdog->Run();
}

unsigned Watchdog::Run() {
  SetThreadName();
  TimeDelta remaining_duration;
  while (1) {
    AutoLock lock(lock_);
    while (DISARMED == state_)
      condition_variable_.Wait();
    if (SHUTDOWN == state_)
      return 0;
    DCHECK(ARMED == state_);
    remaining_duration = duration_ - (TimeTicks::Now() - start_time_);
    if (remaining_duration.InMilliseconds() > 0) {
      // Spurios wake?  Timer drifts?  Go back to sleep for remaining time.
      condition_variable_.TimedWait(remaining_duration);
    } else {
      // We overslept, so this seems like a real alarm.
      // Watch out for a user that stopped the debugger on a different alarm!
      {
        AutoLock static_lock(static_lock_);
        if (last_debugged_alarm_time_ > start_time_) {
          // False alarm: we started our clock before the debugger break (last
          // alarm time).
          start_time_ += last_debugged_alarm_delay_;
          if (last_debugged_alarm_time_ > start_time_)
            state_ = DISARMED;  // Too many alarms must have taken place.
          continue;
        }
      }
      state_ = DISARMED;  // Only alarm at most once.
      TimeTicks last_alarm_time = TimeTicks::Now();
      Alarm();  // Set a break point here to debug on alarms.
      TimeDelta last_alarm_delay = TimeTicks::Now() - last_alarm_time;
      if (last_alarm_delay > TimeDelta::FromMilliseconds(2)) {
        // Ignore race of two alarms/breaks going off at roughly the same time.
        AutoLock static_lock(static_lock_);
        // This was a real debugger break.
        last_debugged_alarm_time_ = last_alarm_time;
        last_debugged_alarm_delay_ = last_alarm_delay;
      }
    }
  }
}

void Watchdog::SetThreadName() const {
  std::string name = StringPrintf("%s Watchdog",
                                  WideToASCII(thread_watched_name_).c_str());
  PlatformThread::SetName(thread_id_, name.c_str());
  DLOG(INFO) << "Watchdog active: " << name;
}

// static
Lock Watchdog::static_lock_;  // Lock for access of static data...
// static
TimeTicks Watchdog::last_debugged_alarm_time_ = TimeTicks();
// static
TimeDelta Watchdog::last_debugged_alarm_delay_;
