// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

BrowserProxy* WindowProxy::GetBrowser() {
  return GetBrowserWithTimeout(INFINITE, NULL);
}

BrowserProxy* WindowProxy::GetBrowserWithTimeout(uint32 timeout_ms,
                                                 bool* is_timeout) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_BrowserForWindowRequest(0, handle_), &response,
    AutomationMsg_BrowserForWindowResponse::ID, timeout_ms, is_timeout);
  if (!succeeded)
    return NULL;

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on exit.
  int browser_handle = 0;
  void* iter = NULL;
  bool handle_ok;
  succeeded = response->ReadBool(&iter, &handle_ok);
  if (succeeded)
    succeeded = response->ReadInt(&iter, &browser_handle);

  if (succeeded) {
    return new BrowserProxy(sender_, tracker_, browser_handle);
  } else {
    return NULL;
  }
}
