// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include "base/logging.h"
#include "chrome/common/worker_thread_ticker.h"

WorkerThreadTicker::WorkerThreadTicker(int tick_interval)
    : tick_interval_(tick_interval),
      wait_handle_(NULL) {
}

WorkerThreadTicker::~WorkerThreadTicker() {
  Stop();
}

bool WorkerThreadTicker::RegisterTickHandler(Callback *tick_handler) {
  DCHECK(tick_handler);
  AutoLock lock(tick_handler_list_lock_);
  // You cannot change the list of handlers when the timer is running.
  // You need to call Stop first.
  if (IsRunning()) {
    return false;
  }
  tick_handler_list_.push_back(tick_handler);
  return true;
}

bool WorkerThreadTicker::UnregisterTickHandler(Callback *tick_handler) {
  DCHECK(tick_handler);
  AutoLock lock(tick_handler_list_lock_);
  // You cannot change the list of handlers when the timer is running.
  // You need to call Stop first.
  if (IsRunning()) {
    return false;
  }
  TickHandlerListType::iterator index = std::remove(tick_handler_list_.begin(),
                                                    tick_handler_list_.end(),
                                                    tick_handler);
  if (index == tick_handler_list_.end()) {
    return false;
  }
  tick_handler_list_.erase(index, tick_handler_list_.end());
  return true;
}

bool WorkerThreadTicker::Start() {
  // Do this in a lock because we don't want 2 threads to
  // call Start at the same time
  AutoLock lock(tick_handler_list_lock_);
  if (IsRunning()) {
    return false;
  }
  bool ret = false;
  HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!event) {
    NOTREACHED();
  } else {
    if (!RegisterWaitForSingleObject(
            &wait_handle_,
            event,
            reinterpret_cast<WAITORTIMERCALLBACK>(TickCallback),
            this,
            tick_interval_,
            WT_EXECUTEDEFAULT)) {
      NOTREACHED();
      CloseHandle(event);
    } else {
      dummy_event_.Attach(event);
      ret = true;
    }
  }
  return ret;
}

bool WorkerThreadTicker::Stop() {
  // Do this in a lock because we don't want 2 threads to
  // call Stop at the same time
  AutoLock lock(tick_handler_list_lock_);
  if (!IsRunning()) {
    return false;
  }
  DCHECK(wait_handle_);

  // Wait for the callbacks to be done. Passing
  // INVALID_HANDLE_VALUE to UnregisterWaitEx achieves this.
  UnregisterWaitEx(wait_handle_, INVALID_HANDLE_VALUE);
  wait_handle_ = NULL;
  dummy_event_.Close();
  return true;
}

bool WorkerThreadTicker::IsRunning() const {
  return (wait_handle_ != NULL);
}

void WorkerThreadTicker::InvokeHandlers() {
  // When the ticker is running, the handler list CANNOT be modified.
  // So we can do the enumeration safely without a lock
  TickHandlerListType::iterator index = tick_handler_list_.begin();
  while (index != tick_handler_list_.end()) {
    (*index)->OnTick();
    index++;
  }
}

void CALLBACK WorkerThreadTicker::TickCallback(WorkerThreadTicker* this_ticker,
                                               BOOLEAN timer_or_wait_fired) {
  DCHECK(NULL != this_ticker);
  if (NULL != this_ticker) {
    this_ticker->InvokeHandlers();
  }
}


