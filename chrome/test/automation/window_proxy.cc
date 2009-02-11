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

  return sender_->Send(new AutomationMsg_WindowHWND(0, handle_, handle));
}

bool WindowProxy::SimulateOSClick(const POINT& click, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowClick(0, handle_, click, flags));
}

bool WindowProxy::SimulateOSKeyPress(wchar_t key, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowKeyPress(0, handle_, key, flags));
}

bool WindowProxy::SetVisible(bool visible) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetWindowVisible(0, handle_, visible,
                                                   &result));
  return result;
}

bool WindowProxy::IsActive(bool* active) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_IsWindowActive(0, handle_, &result, active));
  return result;
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
  if (!is_valid())
    return false;

  if (!bounds) {
    NOTREACHED();
    return false;
  }

  bool result = false;

  sender_->SendWithTimeout(new AutomationMsg_WindowViewBounds(
      0, handle_, view_id, screen_coordinates, &result, bounds),
      timeout_ms, is_timeout);

  return result;
}

bool WindowProxy::GetFocusedViewID(int* view_id) {
  if (!is_valid()) return false;

  if (!view_id) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_GetFocusedViewID(0, handle_,
                                                          view_id));
}

BrowserProxy* WindowProxy::GetBrowser() {
  return GetBrowserWithTimeout(INFINITE, NULL);
}

BrowserProxy* WindowProxy::GetBrowserWithTimeout(uint32 timeout_ms,
                                                 bool* is_timeout) {
  if (!is_valid())
    return false;

  bool handle_ok = false;
  int browser_handle = 0;

  sender_->Send(new AutomationMsg_BrowserForWindow(0, handle_, &handle_ok,
                                                   &browser_handle));
  if (!handle_ok)
    return NULL;

  return new BrowserProxy(sender_, tracker_, browser_handle);
}
