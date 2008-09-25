// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_libevent.h"

#include <fcntl.h>

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/time.h"
#include "third_party/libevent/event.h"

namespace base {

// Return 0 on success
// Too small a function to bother putting in a library?
static int SetNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags)
      flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Called if a byte is received on the wakeup pipe.
void MessagePumpLibevent::OnWakeup(int socket, short flags, void* context) {

  base::MessagePumpLibevent* that = 
              static_cast<base::MessagePumpLibevent*>(context);
  DCHECK(that->wakeup_pipe_out_ == socket);

  // Remove and discard the wakeup byte.
  char buf;
  int nread = read(socket, &buf, 1);
  DCHECK(nread == 1);
  // Tell libevent to break out of inner loop.
  event_base_loopbreak(that->event_base_);
}

MessagePumpLibevent::MessagePumpLibevent()
    : keep_running_(true),
      in_run_(false),
      event_base_(event_base_new()),
      wakeup_pipe_in_(-1),
      wakeup_pipe_out_(-1) {
  if (!Init())
     NOTREACHED();
}

bool MessagePumpLibevent::Init() {
  int fds[2];
  if (pipe(fds))
    return false;
  if (SetNonBlocking(fds[0]))
    return false;
  if (SetNonBlocking(fds[1]))
    return false;
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];

  wakeup_event_ = new event;
  event_set(wakeup_event_, wakeup_pipe_out_, EV_READ | EV_PERSIST, 
	    OnWakeup, this);
  event_base_set(event_base_, wakeup_event_);

  if (event_add(wakeup_event_, 0))
    return false;
  return true;
}

MessagePumpLibevent::~MessagePumpLibevent() {
  DCHECK(wakeup_event_);
  DCHECK(event_base_);
  event_del(wakeup_event_);
  delete wakeup_event_;
  event_base_free(event_base_);
}

void MessagePumpLibevent::WatchSocket(int socket, short interest_mask, 
                                      event* e, Watcher* watcher) {

  // Set current interest mask and message pump for this event
  event_set(e, socket, interest_mask, OnReadinessNotification, watcher);

  // Tell libevent which message pump this socket will belong to when we add it.
  event_base_set(event_base_, e);

  // Add this socket to the list of monitored sockets.
  if (event_add(e, NULL))
    NOTREACHED();
}

void MessagePumpLibevent::UnwatchSocket(event* e) {
  // Remove this socket from the list of monitored sockets.
  if (event_del(e))
    NOTREACHED();
}

void MessagePumpLibevent::OnReadinessNotification(int socket, short flags, 
                                                  void* context) {
  // The given socket is ready for I/O.
  // Tell the owner what kind of I/O the socket is ready for.
  Watcher* watcher = static_cast<Watcher*>(context);
  watcher->OnSocketReady(flags);
}

// Reentrant!
void MessagePumpLibevent::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  bool old_in_run = in_run_;
  in_run_ = true;

  for (;;) {
    ScopedNSAutoreleasePool autorelease_pool;

    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    // EVLOOP_ONCE tells libevent to only block once,
    // but to service all pending events when it wakes up.
    if (delayed_work_time_.is_null()) {
      event_base_loop(event_base_, EVLOOP_ONCE);
    } else {
      TimeDelta delay = delayed_work_time_ - Time::Now();
      if (delay > TimeDelta()) {
        struct timeval poll_tv;
        poll_tv.tv_sec = delay.InSeconds();
        poll_tv.tv_usec = delay.InMicroseconds() % Time::kMicrosecondsPerSecond;
        event_base_loopexit(event_base_, &poll_tv);
        event_base_loop(event_base_, EVLOOP_ONCE);
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = Time();
      }
    }
  }

  keep_running_ = true;
  in_run_ = old_in_run;
}

void MessagePumpLibevent::Quit() {
  DCHECK(in_run_);
  // Tell both libevent and Run that they should break out of their loops.
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpLibevent::ScheduleWork() {
  // Tell libevent (in a threadsafe way) that it should break out of its loop.
  char buf = 0;
  int nwrite = write(wakeup_pipe_in_, &buf, 1);
  DCHECK(nwrite == 1);
}

void MessagePumpLibevent::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base

