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

#include "base/condition_variable.h"

#include <errno.h>
#include <sys/time.h>

#include "base/lock.h"
#include "base/lock_impl.h"
#include "base/logging.h"

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_impl()->os_lock()) {
  int rv = pthread_cond_init(&condition_, NULL);
  DCHECK(rv == 0);
}

ConditionVariable::~ConditionVariable() {
  int rv = pthread_cond_destroy(&condition_);
  DCHECK(rv == 0);
}

void ConditionVariable::Wait() {
  int rv = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK(rv == 0);
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  int64 usecs = max_time.InMicroseconds();

  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timeval now;
  gettimeofday(&now, NULL);

  struct timespec abstime;
  abstime.tv_sec = now.tv_sec + (usecs / Time::kMicrosecondsPerSecond);
  abstime.tv_nsec = (now.tv_usec + (usecs % Time::kMicrosecondsPerSecond)) *
                    Time::kNanosecondsPerMicrosecond;
  abstime.tv_sec += abstime.tv_nsec / Time::kNanosecondsPerSecond;
  abstime.tv_nsec %= Time::kNanosecondsPerSecond;
  DCHECK(abstime.tv_sec >= now.tv_sec);  // Overflow paranoia

  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &abstime);
  DCHECK(rv == 0 || rv == ETIMEDOUT);
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  DCHECK(rv == 0);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&condition_);
  DCHECK(rv == 0);
}
