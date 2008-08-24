// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SHARED_EVENT__
#define BASE_SHARED_EVENT__

#include "base/process_util.h"

class TimeDelta;

typedef HANDLE SharedEventHandle;

class SharedEvent {
 public:
  // Create a new SharedEvent.
  SharedEvent() : event_handle_(NULL) { }

  // Create a SharedEvent from an existing SharedEventHandle.  The new
  // SharedEvent now owns the SharedEventHandle and will close it.
  SharedEvent(SharedEventHandle event_handle) : event_handle_(event_handle) { }
  ~SharedEvent();

  // Create the SharedEvent.
  bool Create(bool manual_reset, bool initial_state);

  // Close the SharedEvent.
  void Close();

  // If |signaled| is true, set the signaled state, otherwise, set to nonsignaled.
  // Returns false if we can't set the signaled state.
  bool SetSignaledState(bool signaled);

  // Returns true if the SharedEvent is signaled.
  bool IsSignaled();

  // Blocks until the event is signaled with a maximum wait time of |timeout|.
  // Returns true if the object is signaled within the timeout.
  bool WaitUntilSignaled(const TimeDelta& timeout);

  // Blocks until the event is signaled.  Returns true if the object is
  // signaled, otherwise an error occurred.
  bool WaitForeverUntilSignaled();

  // Get access to the underlying OS handle for this event.
  SharedEventHandle handle() { return event_handle_; }

  // Share this SharedEvent with |process|.  |new_handle| is an output
  // parameter to receive the handle for use in |process|.  Returns false if we
  // are unable to share the SharedEvent.
  bool ShareToProcess(ProcessHandle process, SharedEventHandle *new_handle);

  // The same as ShareToProcess followed by closing the event.
  bool GiveToProcess(ProcessHandle process, SharedEventHandle *new_handle);

 private:
  SharedEventHandle event_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(SharedEvent);
};

#endif  // BASE_SHARED_EVENT__

