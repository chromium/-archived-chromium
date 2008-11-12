// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_H_
#define BASE_MESSAGE_PUMP_GLIB_H_

#include <glib.h>

#include "base/message_pump.h"
#include "base/scoped_ptr.h"
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
    Delegate* delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;
  };

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
  // The GLib poll structure needs to be owned and freed by us.
  scoped_ptr<GPollFD> work_source_poll_fd_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_H_
