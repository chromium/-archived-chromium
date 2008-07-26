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

#include "chrome/test/automation/window_proxy.h"

#include <vector>
#include <algorithm>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"

bool WindowProxy::GetHWND(HWND* handle) const {
  if (!is_valid()) return false;

  if (!handle) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_WindowHWNDRequest(0, handle_), &response,
    AutomationMsg_WindowHWNDResponse::ID);
  if (!succeeded)
    return false;

  HWND hwnd_response;
  if (AutomationMsg_WindowHWNDResponse::Read(response, &hwnd_response) &&
      hwnd_response) {
    *handle = hwnd_response;
  } else {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

bool WindowProxy::SimulateOSClick(const POINT& click, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowClickRequest(0, handle_, click, flags));
}

bool WindowProxy::SimulateOSKeyPress(wchar_t key, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowKeyPressRequest(0, handle_, key, flags));
}

bool WindowProxy::SetVisible(bool visible) {
  if (!is_valid()) return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetWindowVisibleRequest(0, handle_, visible),
        &response, AutomationMsg_SetWindowVisibleResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Ensure deleted.
  if (!succeeded)
    return false;

  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded))
    succeeded = false;

  return succeeded;
}

bool WindowProxy::IsActive(bool* active) {
  if (!is_valid()) return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_IsWindowActiveRequest(0, handle_),
        &response, AutomationMsg_IsWindowActiveResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Ensure deleted.
  if (!succeeded)
    return false;

  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded) || !succeeded)
    return false;

  if (!response->ReadBool(&iter, active))
    return false;

  return true;
}

bool WindowProxy::Activate() {
  if (!is_valid()) return false;

  return sender_->Send(new AutomationMsg_ActivateWindow(0, handle_));
}

bool WindowProxy::GetViewBounds(int view_id, gfx::Rect* bounds,
                                bool screen_coordinates) {
  return GetViewBoundsWithTimeout(view_id, bounds, screen_coordinates,
                                  INFINITE, NULL);
}

bool WindowProxy::GetViewBoundsWithTimeout(int view_id, gfx::Rect* bounds,
                                           bool screen_coordinates,
                                           uint32 timeout_ms,
                                           bool* is_timeout) {
  if (!is_valid()) return false;

  if (!bounds) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_WindowViewBoundsRequest(0, handle_, view_id,
                                                screen_coordinates),
      &response,
      AutomationMsg_WindowViewBoundsResponse::ID,
      timeout_ms,
      is_timeout);
  if (!succeeded)
    return false;

  Tuple2<bool, gfx::Rect> result;
  AutomationMsg_WindowViewBoundsResponse::Read(response, &result);

  *bounds = result.b;
  return result.a;
}

bool WindowProxy::GetFocusedViewID(int* view_id) {
  if (!is_valid()) return false;

  if (!view_id) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetFocusedViewIDRequest(0, handle_),
      &response,
      AutomationMsg_GetFocusedViewIDResponse::ID);

  *view_id = -1;
  if (succeeded &&
      AutomationMsg_GetFocusedViewIDResponse::Read(response, view_id))
    return true;

  return false;
}