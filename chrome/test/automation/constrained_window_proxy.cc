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

#include "constrained_window_proxy.h"

#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

bool ConstrainedWindowProxy::GetTitle(std::wstring* title) const {
  if (!is_valid())
    return false;

  if (!title) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_ConstrainedTitleRequest(0, handle_), &response,
    AutomationMsg_ConstrainedTitleResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int title_size_response = -1;
  if (response->ReadInt(&iter, &title_size_response) &&
    (title_size_response >= 0)) {
      response->ReadWString(&iter, title);
  } else {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

bool ConstrainedWindowProxy::GetBoundsWithTimeout(gfx::Rect* bounds,
                                                  uint32 timeout_ms,
                                                  bool* is_timeout) {
  if (!is_valid())
    return false;

  if (!bounds) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ConstrainedWindowBoundsRequest(0, handle_),
      &response,
      AutomationMsg_ConstrainedWindowBoundsResponse::ID,
      timeout_ms,
      is_timeout);
  scoped_ptr<IPC::Message> responseDeleter(response);
  if (!succeeded)
    return false;

  Tuple2<bool, gfx::Rect> result;
  AutomationMsg_WindowViewBoundsResponse::Read(response, &result);

  *bounds = result.b;
  return result.a;
}
