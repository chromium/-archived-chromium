// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_PUMP_LIBEVENT_H_

#include "base/message_pump.h"
#include "base/time.h"

// Declare structs we need from libevent.h rather than including it
struct event_base;
struct event;

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
class MessagePumpLibevent : public MessagePump {
 public:
  // Used with WatchObject to asynchronously monitor the I/O readiness of a
  // socket.
  class Watcher {
   public:
    virtual ~Watcher() {}
    // Called from MessageLoop::Run when a ready socket is detected.
    virtual void OnSocketReady(short eventmask) = 0;
  };

  MessagePumpLibevent();
  virtual ~MessagePumpLibevent();

  // Have the current thread's message loop watch for a ready socket.
  // Caller must provide a struct event for this socket for libevent's use.
  // The event and interest_mask fields are defined in libevent.
  // Returns true on success.  
  // TODO(dkegel): hide libevent better; abstraction still too leaky
  // TODO(dkegel): better error handing
  // TODO(dkegel): switch to edge-triggered readiness notification
  void WatchSocket(int socket, short interest_mask, event* e, Watcher*);

  // Stop watching a socket.
  // Event was previously initialized by WatchSocket.
  void UnwatchSocket(event* e);

  // MessagePump methods:
  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const Time& delayed_work_time);

 private:

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // This flag is set to false when Run should return.
  bool keep_running_;

  // This flag is set when inside Run.
  bool in_run_;

  // The time at which we should call DoDelayedWork.
  Time delayed_work_time_;

  // Libevent dispatcher.  Watches all sockets registered with it, and sends 
  // readiness callbacks when a socket is ready for I/O.
  event_base* event_base_;

  // Called by libevent to tell us a registered socket is ready
  static void OnReadinessNotification(int socket, short flags, void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);
  // ... write end; ScheduleWork() writes a single byte to it 
  int wakeup_pipe_in_;
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  int wakeup_pipe_out_;
  // ... libevent wrapper for read end
  event* wakeup_event_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpLibevent);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_LIBEVENT_H_

