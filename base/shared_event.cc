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
