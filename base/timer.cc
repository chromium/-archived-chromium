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

#include "base/timer.h"

#include <math.h>
#if defined(OS_WIN)
#include <mmsystem.h>
#endif

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"

// A sequence number for all allocated times (used to break ties when
// comparing times in the TimerManager, and assure FIFO execution sequence).
static base::AtomicSequenceNumber timer_id_counter_;

//-----------------------------------------------------------------------------
// Timer

Timer::Timer(int delay, Task* task, bool repeating)
    : task_(task),
      delay_(delay),
      repeating_(repeating) {
  timer_id_ = timer_id_counter_.GetNext();
  DCHECK(delay >= 0);
  Reset();
}

int Timer::GetCurrentDelay() const {
  // Be careful here.  Timers have a precision of microseconds, but this API is
  // in milliseconds.  If there are 5.5ms left, should the delay be 5 or 6?  It
  // should be 6 to avoid timers firing early.
  double delay = ceil((fire_time_ - Time::Now()).InMillisecondsF());
  return static_cast<int>(delay);
}

void Timer::Reset() {
  creation_time_ = Time::Now();
  fire_time_ = creation_time_ + TimeDelta::FromMilliseconds(delay_);
  DHISTOGRAM_COUNTS(L"Timer.Durations", delay_);
}

//-----------------------------------------------------------------------------
// TimerPQueue

void TimerPQueue::RemoveTimer(Timer* timer) {
  const std::vector<Timer*>::iterator location =
      find(c.begin(), c.end(), timer);
  if (location != c.end()) {
    c.erase(location);
    make_heap(c.begin(), c.end(), TimerComparison());
  }
}

bool TimerPQueue::ContainsTimer(const Timer* timer) const {
  return find(c.begin(), c.end(), timer) != c.end();
}

//-----------------------------------------------------------------------------
// TimerManager

TimerManager::TimerManager(MessageLoop* message_loop)
    : use_broken_delay_(false),
      message_loop_(message_loop) {
#if defined(OS_WIN)
  // We've experimented with all sorts of timers, and initially tried
  // to avoid using timeBeginPeriod because it does affect the system
  // globally.  However, after much investigation, it turns out that all
  // of the major plugins (flash, windows media 9-11, and quicktime)
  // already use timeBeginPeriod to increase the speed of the clock.
  // Since the browser must work with these plugins, the browser already
  // needs to support a fast clock.  We may as well use this ourselves,
  // as it really is the best timer mechanism for our needs.
  timeBeginPeriod(1);
#endif
}

TimerManager::~TimerManager() {
#if defined(OS_WIN)
  // Match timeBeginPeriod() from construction.
  timeEndPeriod(1);
#endif

  // Be nice to unit tests, and discard and delete all timers along with the
  // embedded task objects by handing off to MessageLoop (which would have Run()
  // and optionally deleted the objects).
  while (timers_.size()) {
    Timer* pending = timers_.top();
    timers_.pop();
    message_loop_->DiscardTimer(pending);
  }
}


Timer* TimerManager::StartTimer(int delay, Task* task, bool repeating) {
  Timer* t = new Timer(delay, task, repeating);
  StartTimer(t);
  return t;
}

void TimerManager::StopTimer(Timer* timer) {
  // Make sure the timer is actually running.
  if (!IsTimerRunning(timer))
    return;
  // Kill the active timer, and remove the pending entry from the queue.
  if (timer != timers_.top()) {
    timers_.RemoveTimer(timer);
  } else {
    timers_.pop();
    DidChangeNextTimer();
  }
}

void TimerManager::ResetTimer(Timer* timer) {
  StopTimer(timer);
  timer->Reset();
  StartTimer(timer);
}

bool TimerManager::IsTimerRunning(const Timer* timer) const {
  return timers_.ContainsTimer(timer);
}

Timer* TimerManager::PeekTopTimer() {
  if (timers_.empty())
    return NULL;
  return timers_.top();
}

bool TimerManager::RunSomePendingTimers() {
  bool did_work = false;
  // Process a small group of timers.  Cap the maximum number of timers we can
  // process so we don't deny cycles to other parts of the process when lots of
  // timers have been set.
  const int kMaxTimersPerCall = 2;
  for (int i = 0; i < kMaxTimersPerCall; ++i) {
    if (timers_.empty() || GetCurrentDelay() > 0)
      break;

    // Get a pending timer.  Deal with updating the timers_ queue and setting
    // the TopTimer.  We'll execute the timer task only after the timer queue
    // is back in a consistent state.
    Timer* pending = timers_.top();

    // If pending task isn't invoked_later, then it must be possible to run it
    // now (i.e., current task needs to be reentrant).
    // TODO(jar): We may block tasks that we can queue from being popped.
    if (!message_loop_->NestableTasksAllowed() &&
        !pending->task()->is_owned_by_message_loop())
      break;

    timers_.pop();
    did_work = true;

    // If the timer is repeating, add it back to the list of timers to process.
    if (pending->repeating()) {
      pending->Reset();
      timers_.push(pending);
    }

    message_loop_->RunTimerTask(pending);
  }

  // Restart the WM_TIMER (if necessary).
  if (did_work)
    DidChangeNextTimer();

  return did_work;
}

// Note: Caller is required to call timer->Reset() before calling StartTimer().
// TODO(jar): change API so that Reset() is called as part of StartTimer, making
// the API a little less error prone.
void TimerManager::StartTimer(Timer* timer) {
  // Make sure the timer is not running.
  if (IsTimerRunning(timer))
    return;

  timers_.push(timer);  // Priority queue will sort the timer into place.

  if (timers_.top() == timer)  // We are new head of queue.
    DidChangeNextTimer();
}

int TimerManager::GetCurrentDelay() {
  if (timers_.empty())
    return -1;
  int delay = timers_.top()->GetCurrentDelay();
  if (delay < 0)
    delay = 0;
  return delay;
}

void TimerManager::DidChangeNextTimer() {
  // Determine if the next timer expiry actually changed...
  if (!timers_.empty()) {
    const Time& expiry = timers_.top()->fire_time();
    if (expiry == next_timer_expiry_)
      return;
    next_timer_expiry_ = expiry;
  } else {
    next_timer_expiry_ = Time();
  }
  message_loop_->DidChangeNextTimerExpiry();
}

//-----------------------------------------------------------------------------
// SimpleTimer

SimpleTimer::SimpleTimer(TimeDelta delay, Task* task, bool repeating)
    : timer_(static_cast<int>(delay.InMilliseconds()), task, repeating),
      owns_task_(true) {
}

SimpleTimer::~SimpleTimer() {
  Stop();

  if (owns_task_)
    delete timer_.task();
}

void SimpleTimer::Start() {
  DCHECK(timer_.task());
  timer_.Reset();
  MessageLoop::current()->timer_manager()->StartTimer(&timer_);
}

void SimpleTimer::Stop() {
  MessageLoop::current()->timer_manager()->StopTimer(&timer_);
}

bool SimpleTimer::IsRunning() const {
  return MessageLoop::current()->timer_manager()->IsTimerRunning(&timer_);
}

void SimpleTimer::Reset() {
  DCHECK(timer_.task());
  MessageLoop::current()->timer_manager()->ResetTimer(&timer_);
}
