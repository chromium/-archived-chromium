// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
