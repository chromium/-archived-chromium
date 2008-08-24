// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_event.h"
#include "base/logging.h"
#include "base/time.h"

SharedEvent::~SharedEvent() {
  Close();
}

bool SharedEvent::Create(bool manual_reset, bool initial_state) {
  DCHECK(!event_handle_);
  event_handle_ = CreateEvent(NULL /* security attributes */, manual_reset,
                              initial_state, NULL /* name */);
  DCHECK(event_handle_);
  return !!event_handle_;
}

void SharedEvent::Close() {
  if (event_handle_) {
    BOOL rv = CloseHandle(event_handle_);
    DCHECK(rv);
    event_handle_ = NULL;
  }
}

bool SharedEvent::SetSignaledState(bool signaled) {
  DCHECK(event_handle_);
  BOOL rv;
  if (signaled) {
    rv = SetEvent(event_handle_);
  } else {
    rv = ResetEvent(event_handle_);
  }
  return rv ? true : false;
}

bool SharedEvent::IsSignaled() {
  DCHECK(event_handle_);
  DWORD event_state = ::WaitForSingleObject(event_handle_, 0);
  DCHECK(WAIT_OBJECT_0 == event_state || WAIT_TIMEOUT == event_state);
  return event_state == WAIT_OBJECT_0;
}

bool SharedEvent::WaitUntilSignaled(const TimeDelta& timeout) {
  DCHECK(event_handle_);
  DWORD event_state = ::WaitForSingleObject(event_handle_,
      static_cast<DWORD>(timeout.InMillisecondsF()));
  return event_state == WAIT_OBJECT_0;
}

bool SharedEvent::WaitForeverUntilSignaled() {
  DCHECK(event_handle_);
  DWORD event_state = ::WaitForSingleObject(event_handle_,
                                            INFINITE);
  return event_state == WAIT_OBJECT_0;
}

bool SharedEvent::ShareToProcess(ProcessHandle process,
                                 SharedEventHandle *new_handle) {
  DCHECK(event_handle_);
  HANDLE event_handle_copy;
  BOOL rv = DuplicateHandle(GetCurrentProcess(), event_handle_, process,
      &event_handle_copy, 0, FALSE, DUPLICATE_SAME_ACCESS);

  if (rv)
    *new_handle = event_handle_copy;
  return rv ? true : false;
}

bool SharedEvent::GiveToProcess(ProcessHandle process,
                                SharedEventHandle *new_handle) {
  bool rv = ShareToProcess(process, new_handle);
  if (rv)
    Close();
  return rv;
}

