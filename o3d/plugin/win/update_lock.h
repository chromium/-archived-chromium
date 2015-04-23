/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_PLUGIN_WIN_UPDATE_LOCK_H_
#define O3D_PLUGIN_WIN_UPDATE_LOCK_H_

#if !defined(O3D_INTERNAL_PLUGIN)

#include <windows.h>
#include "base/basictypes.h"

namespace update_lock {

const TCHAR kRunningEventName[] =
    L"Global\\{AA4817F6-5DB2-482f-92E9-6BD2FF9F3B14}";

class HandleWrapper {
 public:
  HandleWrapper(HANDLE handle) {
    handle_ = handle;
  }
  ~HandleWrapper() {
    CloseHandle(handle_);
  }
  HANDLE handle() {
    return handle_;
  }
 private:
  HANDLE handle_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HandleWrapper);
}; // HandleWrapper

// Returns true if the software is currently not running and can be updated.
bool CanUpdate() {
  // Look for Global kernel object created when application is running
  HandleWrapper running_event(
      OpenEvent(EVENT_ALL_ACCESS, FALSE, kRunningEventName));
  if (NULL != running_event.handle()) {
    return false;
  }
  return true;
}

// Returns a HANDLE that should be closed when the software is shutting down.
HANDLE LockFromUpdates() {
  SECURITY_ATTRIBUTES* security_attributes = NULL;
  SECURITY_ATTRIBUTES security_attributes_buffer;
  SECURITY_DESCRIPTOR security_descriptor;

  ZeroMemory(&security_attributes_buffer, sizeof(security_attributes_buffer));
  security_attributes_buffer.nLength = sizeof(security_attributes_buffer);
  security_attributes_buffer.bInheritHandle = FALSE;

  // initialize the security descriptor and give it a NULL DACL
  if (InitializeSecurityDescriptor(&security_descriptor,
                                   SECURITY_DESCRIPTOR_REVISION)) {
    if (SetSecurityDescriptorDacl(&security_descriptor, TRUE,
                                  (PACL)NULL, FALSE)) {
      security_attributes_buffer.lpSecurityDescriptor = &security_descriptor;
      security_attributes = &security_attributes_buffer;
    }
  }
  // An event is used to lock out updates since events are closed by the OS
  // if their process dies.  So, updates can still happen if an O3D executable
  // crashes (as long as all instances of the running event are closed, either
  // properly or due to a crash).
  return CreateEvent(security_attributes, FALSE, FALSE, kRunningEventName);
}
}  // namespace update_lock

#endif  // O3D_INTERNAL_PLUGIN

#endif  // O3D_PLUGIN_WIN_UPDATE_LOCK_H_

