// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_H_
#define BASE_MESSAGE_PUMP_GLIB_H_

#include <glib.h>

#include "base/message_pump.h"
#include "base/time.h"

namespace base {

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_LINUX platforms using GLib.
class MessagePumpForUI : public MessagePump {
 public:
  MessagePumpForUI();
  ~MessagePumpForUI();

  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const Time& delayed_work_time);

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState {
    // This is the delegate argument passed to Run.
    Delegate* delegate;
    // This tells us when to exit the event pump.
    bool keep_running;
    // This tells our work source when to dispatch DoWork and DoDelayedWork.
    bool should_do_work;
    // This tells our idle source when to dispatch DoIdleWork.
    bool should_do_idle_work;
    // Unlike the work source, which is shared by all calls to Run, each Run
    // call gets its own idle source because we need to destroy it when we have
    // no idle work, and we don't want to destroy someone else's source.
    GSource* idle_source;
  };

  struct WorkSource : GSource {
    MessagePumpForUI* self;
  };

  // The source with these callbacks remain in the main loop forever.  They
  // will dispatch DoWork and DoDelayedWork, and calculate when and how long
  // to block when GLib calls poll internally.
  static GSourceFuncs WorkSourceFuncs;
  static gboolean WorkSourcePrepare(GSource* source, gint* timeout_ms);
  static gboolean WorkSourceCheck(GSource* source);
  static gboolean WorkSourceDispatch(GSource* source, GSourceFunc unused_func,
                                     gpointer unused_data);

  // The source that uses these callbacks is added as an idle source, which
  // means GLib will call it when there is no other work to do.  We continue
  // doing work as long as DoIdleWork or the other work functions return true.
  // Once no work remains, we remove the idle source so GLib will block instead
  // of firing it.  Then we re-add it when we wake up.
  static GSourceFuncs IdleSourceFuncs;
  static gboolean IdleSourcePrepare(GSource* source, gint* timeout_ms);
  static gboolean IdleSourceCheck(GSource* source);
  static gboolean IdleSourceDispatch(GSource* source, GSourceFunc unused_func,
                                     gpointer unused_data);

  // This adds a GLib source to the main loop.
  GSource* AddSource(GSourceFuncs* funcs, gint priority,
                     GPollFD* optional_poll_fd);

  RunState* state_;

  // This is a GLib structure that we can add event sources to.  We use the
  // default GLib context, which is the one to which all GTK events are
  // dispatched.
  GMainContext* context_;

  // This is the time when we need to do delayed work.
  Time delayed_work_time_;

  // We use a pipe to schedule work in a thread-safe way that doesn't interfere
  // with our state.  When ScheduleWork is called, we write into the pipe which
  // ensures poll will not sleep, since we use the read end as an event source.
  // When we find data pending on the pipe, we clear it out and know we have
  // been given new work.
  int write_fd_work_scheduled_;
  int read_fd_work_scheduled_;

  // The work source.  It is shared by all calls to Run and destroyed when
  // the message pump is destroyed.
  GSource* work_source_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_H_
