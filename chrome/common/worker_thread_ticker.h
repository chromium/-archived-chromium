// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WORKER_THREAD_TICKER_H__
#define CHROME_COMMON_WORKER_THREAD_TICKER_H__

#include <windows.h>
#include <atlbase.h>
#include <vector>

#include "base/lock.h"

// This class provides the follwoing functionality:
// It invokes a set of registered handlers at periodic intervals in
// the context of an arbitrary worker thread.
// This functionality is similar to a waitable timer except that the
// timer in this case is a low-resolution timer (millisecond granularity)
// and it does not require the caller to be in an alertable wait state.
// The callbacks are invoked in the context of an arbitrary worker thread
// from the system thread pool
class WorkerThreadTicker {
 public:
  // This callback interface to be implemented by clients of this
  // class
  class Callback {
   public:
    // Gets invoked when the timer period is up
    virtual void OnTick() = 0;
  };

  // tick_interval is the periodic interval in which to invoke the
  // registered handlers
  explicit WorkerThreadTicker(int tick_interval);

  ~WorkerThreadTicker();

  // Registers a callback handler interface
  // tick_handler is the handler interface to register. The ownership of this
  // object is not transferred to this class.
  bool RegisterTickHandler(Callback *tick_handler);

  // Unregisters a callback handler interface
  // tick_handler is the handler interface to unregister
  bool UnregisterTickHandler(Callback *tick_handler);

  // Starts the ticker. Returns false if the ticker is already running
  // or if the Start fails.
  bool Start();
  // Stops the ticker and waits for all callbacks. to be done. This method
  // does not provide a way to stop without waiting for the callbacks to be
  // done because this is inherently risky.
  // Returns false is the ticker is not running
  bool Stop();
  bool IsRunning() const;

  void set_tick_interval(int tick_interval) {
    tick_interval_ = tick_interval;
  }

  int tick_interval() const {
    return tick_interval_;
  }

 private:
  // A list type that holds all registered callback interfaces
  typedef std::vector<Callback*> TickHandlerListType;

  // This is the callback function registered with the
  // RegisterWaitForSingleObject API. It gets invoked in a system worker thread
  // periodically at intervals of tick_interval_ miliiseconds
  static void CALLBACK TickCallback(WorkerThreadTicker* this_ticker,
                                    BOOLEAN timer_or_wait_fired);

  // Helper that invokes all registered handlers
  void InvokeHandlers();

  // A dummy event to be used by the RegisterWaitForSingleObject API
  CHandle dummy_event_;
  // The wait handle returned by the RegisterWaitForSingleObject API
  HANDLE wait_handle_;
  // The interval at which the callbacks are to be invoked
  int tick_interval_;
  // Lock for the tick_handler_list_ list
  Lock tick_handler_list_lock_;
  // A list that holds all registered callback interfaces
  TickHandlerListType tick_handler_list_;

  DISALLOW_EVIL_CONSTRUCTORS(WorkerThreadTicker);
};

#endif  // CHROME_COMMON_WORKER_THREAD_TICKER_H__

