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
#include <mmsystem.h>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"

// Note about hi-resolution timers.
// This class would *like* to provide high resolution timers.  Windows timers
// using SetTimer() have a 10ms granularity.  We have to use WM_TIMER as a
// wakeup mechanism because the application can enter modal windows loops where
// it is not running our MessageLoop; the only way to have our timers fire in
// these cases is to post messages there.
//
// To provide sub-10ms timers, we process timers directly from our main
// MessageLoop.  For the common case, timers will be processed there as the
// message loop does its normal work.  However, we *also* set the system timer
// so that WM_TIMER events fire.  This mops up the case of timers not being
// able to work in modal message loops.  It is possible for the SetTimer to
// pop and have no pending timers, because they could have already been
// processed by the message loop itself.
//
// We use a single SetTimer corresponding to the timer that will expire
// soonest.  As new timers are created and destroyed, we update SetTimer.
// Getting a spurrious SetTimer event firing is benign, as we'll just be
// processing an empty timer queue.

static const wchar_t kWndClass[] = L"Chrome_TimerMessageWindow";

static LRESULT CALLBACK MessageWndProc(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  if (message == WM_TIMER) {
    // Timer not firing? Maybe you're suffering from a WM_PAINTstorm. Make sure
    // any WM_PAINT handler you have calls BeginPaint and EndPaint to validate
    // the invalid region, otherwise you will be flooded with paint messages
    // that trump WM_TIMER when PeekMessage is called.
    UINT_PTR timer_id = static_cast<UINT_PTR>(wparam);
    TimerManager* tm = reinterpret_cast<TimerManager*>(timer_id);
    return tm->MessageWndProc(hwnd, message, wparam, lparam);
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

// A sequence number for all allocated times (used to break ties when
// comparing times in the TimerManager, and assure FIFO execution sequence).
static base::AtomicSequenceNumber timer_id_counter_;

//-----------------------------------------------------------------------------
// Timer

Timer::Timer(int delay, Task* task, bool repeating)
    : delay_(delay),
      task_(task),
      repeating_(repeating) {
  timer_id_ = timer_id_counter_.GetNext();
  DCHECK(delay >= 0);
  Reset();
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

TimerManager::TimerManager()
    : use_broken_delay_(false),
      message_hwnd_(NULL),
      use_native_timers_(true),
      message_loop_(NULL) {
  // We've experimented with all sorts of timers, and initially tried
  // to avoid using timeBeginPeriod because it does affect the system
  // globally.  However, after much investigation, it turns out that all
  // of the major plugins (flash, windows media 9-11, and quicktime)
  // already use timeBeginPeriod to increase the speed of the clock.
  // Since the browser must work with these plugins, the browser already
  // needs to support a fast clock.  We may as well use this ourselves,
  // as it really is the best timer mechanism for our needs.
  timeBeginPeriod(1);

  // Initialize the Message HWND in the constructor so that the window
  // belongs to the same thread as the message loop (this is important!)
  GetMessageHWND();
}

TimerManager::~TimerManager() {
  // Match timeBeginPeriod() from construction.
  timeEndPeriod(1);

  if (message_hwnd_ != NULL)
    DestroyWindow(message_hwnd_);

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
    UpdateWindowsWmTimer();  // We took away the head of our queue.
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
  bool allowed_to_run = message_loop()->NestableTasksAllowed();
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
    if (!message_loop()->NestableTasksAllowed() &&
        !pending->task()->is_owned_by_message_loop())
      break;

    timers_.pop();
    did_work = true;

    // If the timer is repeating, add it back to the list of timers to process.
    if (pending->repeating()) {
      pending->Reset();
      timers_.push(pending);
    }

    message_loop()->RunTimerTask(pending);
  }

  // Restart the WM_TIMER (if necessary).
  if (did_work)
    UpdateWindowsWmTimer();

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

  if (timers_.top() == timer)
    UpdateWindowsWmTimer();  // We are new head of queue.
}

void TimerManager::UpdateWindowsWmTimer() {
  if (!use_native_timers_)
    return;

  if (timers_.empty()) {
    KillTimer(GetMessageHWND(), reinterpret_cast<UINT_PTR>(this));
    return;
  }

  int delay = GetCurrentDelay();
  if (delay < USER_TIMER_MINIMUM)
    delay = USER_TIMER_MINIMUM;

  // Simulates malfunctioning, early firing timers. Pending tasks should
  // only be invoked when the delay they specify has elapsed.
  if (use_broken_delay_)
    delay = 10;

  // Create a WM_TIMER event that will wake us up to check for any pending
  // timers (in case the message loop was otherwise starving us).
  SetTimer(GetMessageHWND(), reinterpret_cast<UINT_PTR>(this), delay, NULL);
}

int TimerManager::GetCurrentDelay() {
  if (timers_.empty())
    return -1;
  int delay = timers_.top()->current_delay();
  if (delay < 0)
    delay = 0;
  return delay;
}

int TimerManager::MessageWndProc(HWND hwnd, UINT message, WPARAM wparam,
                                 LPARAM lparam) {
  DCHECK(!lparam);
  DCHECK(this == message_loop()->timer_manager());
  if (message_loop()->NestableTasksAllowed())
    RunSomePendingTimers();
  else
    UpdateWindowsWmTimer();
  return 0;
}

MessageLoop* TimerManager::message_loop() {
  if (!message_loop_)
    message_loop_ = MessageLoop::current();
  DCHECK(message_loop_ == MessageLoop::current());
  return message_loop_;
}


HWND TimerManager::GetMessageHWND() {
  if (!message_hwnd_) {
    HINSTANCE hinst = GetModuleHandle(NULL);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ::MessageWndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kWndClass;
    RegisterClassEx(&wc);

    message_hwnd_ = CreateWindow(kWndClass, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                                 hinst, 0);
    DCHECK(message_hwnd_);
  }
  return message_hwnd_;
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
