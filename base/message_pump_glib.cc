// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib.h"

#include <fcntl.h>
#include <math.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/platform_thread.h"

namespace base {

static const char kWorkScheduled = '\0';
static const char kDelayedWorkScheduled = '\1';

// I wish these could be const, but g_source_new wants a non-const GSourceFunc
// pointer.

// static
GSourceFuncs MessagePumpForUI::WorkSourceFuncs = {
  WorkSourcePrepare,
  WorkSourceCheck,
  WorkSourceDispatch,
  NULL
};

// static
GSourceFuncs MessagePumpForUI::IdleSourceFuncs = {
  IdleSourcePrepare,
  IdleSourceCheck,
  IdleSourceDispatch,
  NULL
};

static int GetTimeIntervalMilliseconds(Time from) {
  if (from.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  double timeout = ceil((from - Time::Now()).InMillisecondsF());

  // If this value is negative, then we need to run delayed work soon.
  int delay = static_cast<int>(timeout);
  if (delay < 0)
    delay = 0;

  return delay;
}

MessagePumpForUI::MessagePumpForUI()
    : state_(NULL),
      context_(g_main_context_default()) {
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
  GPollFD poll_fd;
  poll_fd.fd = read_fd_work_scheduled_;
  poll_fd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  work_source_ = AddSource(&WorkSourceFuncs, G_PRIORITY_DEFAULT, &poll_fd);
}

MessagePumpForUI::~MessagePumpForUI() {
  close(read_fd_work_scheduled_);
  close(write_fd_work_scheduled_);
  g_source_destroy(work_source_);
  g_source_unref(work_source_);
}

struct ThreadIdTraits {
  static void New(void* instance) {
    int* thread_id = static_cast<int*>(instance);
    *thread_id = PlatformThread::CurrentId();
  }
  static void Delete(void* instance) {
  }
};

void MessagePumpForUI::Run(Delegate* delegate) {
  // Make sure we only run this on one thread.  GTK only has one message pump
  // so we can only have one UI loop per process.
  static LazyInstance<int, ThreadIdTraits> thread_id(base::LINKER_INITIALIZED);
  DCHECK(thread_id.Get() == PlatformThread::CurrentId()) <<
      "Running MessagePumpForUI on two different threads; "
      "this is unsupported by GLib!";

  RunState state;
  state.delegate = delegate;
  state.keep_running = true;
  // We emulate the behavior of MessagePumpDefault and try to do work at once.
  state.should_do_work = true;
  state.should_do_idle_work = false;
  state.idle_source = NULL;

  RunState* previous_state = state_;
  state_ = &state;

  while (state.keep_running)
    g_main_context_iteration(context_, true);

  if (state.idle_source) {
    // This removes the source from the context and releases GLib's hold on it.
    g_source_destroy(state.idle_source);
    // This releases our hold and destroys the source.
    g_source_unref(state.idle_source);
  }

  state_ = previous_state;
}

void MessagePumpForUI::Quit() {
  DCHECK(state_) << "Quit called outside Run!";
  state_->keep_running = false;
}

void MessagePumpForUI::ScheduleWork() {
  // This can be called on any thread, so we don't want to touch any state
  // variables as we would then need locks all over.  This ensures that if
  // we are sleeping in a poll that we will wake up, and we check the pipe
  // so we know when work was scheduled.
  CHECK(1 == write(write_fd_work_scheduled_, &kWorkScheduled, 1)) <<
      "Could not write to pipe!";
}

void MessagePumpForUI::ScheduleDelayedWork(const Time& delayed_work_time) {
  delayed_work_time_ = delayed_work_time;
  // This is an optimization.  Delayed work may not be imminent, we may just
  // need to update our timeout to poll.  Hence we don't want to go overkill
  // with kWorkScheduled.
  CHECK(1 == write(write_fd_work_scheduled_, &kDelayedWorkScheduled, 1)) <<
      "Could not write to pipe!";
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

// static
gboolean MessagePumpForUI::WorkSourcePrepare(GSource* source,
                                             gint* timeout_ms) {
  MessagePumpForUI* self = static_cast<WorkSource*>(source)->self;
  RunState* state = self->state_;

  if (state->should_do_work) {
    state->should_do_idle_work = false;
    *timeout_ms = 0;
    return TRUE;
  }

  *timeout_ms = GetTimeIntervalMilliseconds(self->delayed_work_time_);

  state->should_do_idle_work = true;
  // We want to do idle work right before poll goes to sleep.  Obviously
  // we are not currently asleep, but we may be about to since we have
  // no work to do.  If we don't have an idle source ready to go it's
  // probably because it fired already (or we just started and it hasn't
  // been added yet) and we should add one for when we are ready.  Note
  // that this new source will get Prepare called on this current pump
  // iteration since it gets added at the end of the source list.
  if (!state->idle_source) {
    state->idle_source =
        self->AddSource(&IdleSourceFuncs, G_PRIORITY_DEFAULT_IDLE, NULL);
  }

  return FALSE;
}

// static
gboolean MessagePumpForUI::WorkSourceCheck(GSource* source) {
  MessagePumpForUI* self = static_cast<WorkSource*>(source)->self;
  RunState* state = self->state_;

  // Make sure we don't attempt idle work until we are really sure we don't
  // have other work to do.  We'll know this in the call to Prepare.
  state->should_do_idle_work = false;

  // Check if ScheduleWork or ScheduleDelayedWork was called.  This is a
  // non-blocking read.
  char byte;
  while (0 < read(self->read_fd_work_scheduled_, &byte, 1)) {
    // Don't assume we actually have work yet unless the stronger ScheduleWork
    // was called.
    if (byte == kWorkScheduled)
      state->should_do_work = true;
  }

  if (state->should_do_work)
    return TRUE;

  if (!self->delayed_work_time_.is_null())
    return self->delayed_work_time_ <= Time::Now();

  return FALSE;
}

// static
gboolean MessagePumpForUI::WorkSourceDispatch(GSource* source,
                                              GSourceFunc unused_func,
                                              gpointer unused_data) {
  MessagePumpForUI* self = static_cast<WorkSource*>(source)->self;
  RunState* state = self->state_;
  DCHECK(!state->should_do_idle_work) <<
      "Idle work should not be flagged while regular work exists.";

  // Note that in this function we never return FALSE.  This source is owned
  // by GLib and shared by multiple calls to Run.  It will only finally get
  // destroyed when the loop is destroyed.

  state->should_do_work = state->delegate->DoWork();
  if (!state->keep_running)
    return TRUE;

  state->should_do_work |=
      state->delegate->DoDelayedWork(&self->delayed_work_time_);

  return TRUE;
}

// static
gboolean MessagePumpForUI::IdleSourcePrepare(GSource* source,
                                             gint* timeout_ms) {
  RunState* state = static_cast<WorkSource*>(source)->self->state_;
  *timeout_ms = 0;
  return state->should_do_idle_work;
}

// static
gboolean MessagePumpForUI::IdleSourceCheck(GSource* source) {
  RunState* state = static_cast<WorkSource*>(source)->self->state_;
  return state->should_do_idle_work;
}

// static
gboolean MessagePumpForUI::IdleSourceDispatch(GSource* source,
                                              GSourceFunc unused_func,
                                              gpointer unused_data) {
  RunState* state = static_cast<WorkSource*>(source)->self->state_;
  // We should not do idle work unless we didn't have other work to do.
  DCHECK(!state->should_do_work) << "Doing idle work in non-idle time!";
  state->should_do_idle_work = false;
  state->should_do_work = state->delegate->DoIdleWork();

  // This is an optimization.  We could always remove ourselves right now,
  // but we will just get re-added when WorkSourceCheck eventually returns
  // FALSE.
  if (!state->should_do_work) {
    // This is so that when we return FALSE, GLib will not only remove us
    // from the context, but since it holds the last reference, it will
    // destroy us as well.
    g_source_unref(source);
    state->idle_source = NULL;
  }

  return state->should_do_work;
}

GSource* MessagePumpForUI::AddSource(GSourceFuncs* funcs, gint priority,
                                     GPollFD *optional_poll_fd) {
  GSource* source = g_source_new(funcs, sizeof(WorkSource));

  // Setting the priority is actually a bit expensive since it causes GLib
  // to resort an internal list.
  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority(source, priority);

  // This is needed to allow Run calls inside Dispatch.
  g_source_set_can_recurse(source, TRUE);
  static_cast<WorkSource*>(source)->self = this;

  if (optional_poll_fd)
    g_source_add_poll(source, optional_poll_fd);

  g_source_attach(source, context_);
  return source;
}

}  // namespace base
