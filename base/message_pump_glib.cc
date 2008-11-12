// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib.h"

#include <fcntl.h>
#include <math.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/platform_thread.h"

namespace {

// We send a byte across a pipe to wakeup the event loop.
const char kWorkScheduled = '\0';

// Return a timeout suitable for the glib loop, -1 to block forever,
// 0 to return right away, or a timeout in milliseconds from now.
int GetTimeIntervalMilliseconds(base::Time from) {
  if (from.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  int delay = static_cast<int>(
      ceil((from - base::Time::Now()).InMillisecondsF()));

  // If this value is negative, then we need to run delayed work soon.
  return delay < 0 ? 0 : delay;
}

// A brief refresher on GLib:
//     GLib sources have four callbacks: Prepare, Check, Dispatch and Finalize.
// On each iteration of the GLib pump, it calls each source's Prepare function.
// This function should return TRUE if it wants GLib to call its Dispatch, and
// FALSE otherwise.  It can also set a timeout in this case for the next time
// Prepare should be called again (it may be called sooner).
//     After the Prepare calls, GLib does a poll to check for events from the
// system.  File descriptors can be attached to the sources.  The poll may block
// if none of the Prepare calls returned TRUE.  It will block indefinitely, or
// by the minimum time returned by a source in Prepare.
//     After the poll, GLib calls Check for each source that returned FALSE
// from Prepare.  The return value of Check has the same meaning as for Prepare,
// making Check a second chance to tell GLib we are ready for Dispatch.
//     Finally, GLib calls Dispatch for each source that is ready.  If Dispatch
// returns FALSE, GLib will destroy the source.  Dispatch calls may be recursive
// (i.e., you can call Run from them), but Prepare and Check cannot.
//     Finalize is called when the source is destroyed.

struct WorkSource : public GSource {
  int timeout_ms;
};

gboolean WorkSourcePrepare(GSource* source,
                           gint* timeout_ms) {
  *timeout_ms = static_cast<WorkSource*>(source)->timeout_ms;
  return FALSE;
}

gboolean WorkSourceCheck(GSource* source) {
  return FALSE;
}

gboolean WorkSourceDispatch(GSource* source,
                            GSourceFunc unused_func,
                            gpointer unused_data) {
  NOTREACHED();
  return TRUE;
}

// I wish these could be const, but g_source_new wants non-const.
GSourceFuncs WorkSourceFuncs = {
  WorkSourcePrepare,
  WorkSourceCheck,
  WorkSourceDispatch,
  NULL
};

}  // namespace


namespace base {

MessagePumpForUI::MessagePumpForUI()
    : state_(NULL),
      context_(g_main_context_default()),
      work_source_poll_fd_(new GPollFD) {
  // Create a pipe with a non-blocking read end for use by ScheduleWork to
  // break us out of a poll.  Create the work source and attach the file
  // descriptor to it.
  int pipe_fd[2];
  CHECK(0 == pipe(pipe_fd)) << "Could not create pipe!";
  write_fd_work_scheduled_ = pipe_fd[1];
  read_fd_work_scheduled_ = pipe_fd[0];
  int flags = fcntl(read_fd_work_scheduled_, F_GETFL, 0);
  if (-1 == flags)
    flags = 0;
  CHECK(0 == fcntl(read_fd_work_scheduled_, F_SETFL, flags | O_NONBLOCK)) <<
      "Could not set file descriptor to non-blocking!";
  work_source_poll_fd_->fd = read_fd_work_scheduled_;
  work_source_poll_fd_->events = G_IO_IN | G_IO_HUP | G_IO_ERR;

  work_source_ = g_source_new(&WorkSourceFuncs, sizeof(WorkSource));
  // This is needed to allow Run calls inside Dispatch.
  g_source_set_can_recurse(work_source_, TRUE);
  g_source_add_poll(work_source_, work_source_poll_fd_.get());
  g_source_attach(work_source_, context_);
}

MessagePumpForUI::~MessagePumpForUI() {
  close(read_fd_work_scheduled_);
  close(write_fd_work_scheduled_);
  g_source_destroy(work_source_);
  g_source_unref(work_source_);
}

void MessagePumpForUI::Run(Delegate* delegate) {
#ifndef NDEBUG
  // Make sure we only run this on one thread.  GTK only has one message pump
  // so we can only have one UI loop per process.
  static int thread_id = PlatformThread::CurrentId();
  DCHECK(thread_id == PlatformThread::CurrentId()) <<
      "Running MessagePumpForUI on two different threads; "
      "this is unsupported by GLib!";
#endif

  RunState state;
  state.delegate = delegate;
  state.should_quit = false;
  state.run_depth = state_ ? state_->run_depth + 1 : 1;

  RunState* previous_state = state_;
  state_ = &state;

  // We really only do a single task for each iteration of the loop.  If we
  // have done something, assume there is likely something more to do.  This
  // will mean that we don't block on the message pump until there was nothing
  // more to do.  We also set this to true to make sure not to block on the
  // first iteration of the loop, so RunAllPending() works correctly.
  bool more_work_is_plausible = true;
  for (;;) {
    // Set up our timeout for any delayed work.
    static_cast<WorkSource*>(work_source_)->timeout_ms =
        GetTimeIntervalMilliseconds(delayed_work_time_);

    // Process a single iteration of the event loop.
    g_main_context_iteration(context_, !more_work_is_plausible);
    if (state_->should_quit)
      break;

    more_work_is_plausible = false;

    // Drain our wakeup pipe, this is a non-blocking read.
    char tempbuf[16];
    while (read(read_fd_work_scheduled_, tempbuf, sizeof(tempbuf)) > 0) { }

    if (state_->delegate->DoWork())
      more_work_is_plausible = true;

    if (state_->should_quit)
      break;

    if (state_->delegate->DoDelayedWork(&delayed_work_time_))
      more_work_is_plausible = true;
    if (state_->should_quit)
      break;

    // Don't do idle work if we think there are more important things
    // that we could be doing.
    if (more_work_is_plausible)
      continue;

    if (state_->delegate->DoIdleWork())
      more_work_is_plausible = true;
    if (state_->should_quit)
      break;
  }

  state_ = previous_state;
}

void MessagePumpForUI::Quit() {
  if (state_) {
    state_->should_quit = true;
  } else {
    NOTREACHED() << "Quit called outside Run!";
  }
}

void MessagePumpForUI::ScheduleWork() {
  // This can be called on any thread, so we don't want to touch any state
  // variables as we would then need locks all over.  This ensures that if
  // we are sleeping in a poll that we will wake up, and we check the pipe
  // so we know when work was scheduled.
  if (write(write_fd_work_scheduled_, &kWorkScheduled, 1) != 1) {
    NOTREACHED() << "Could not write to the UI message loop wakeup pipe!";
  }
}

void MessagePumpForUI::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We need to wake up the loop in case the poll timeout needs to be
  // adjusted.  This will cause us to try to do work, but that's ok.
  delayed_work_time_ = delayed_work_time;
  ScheduleWork();
}

}  // namespace base
