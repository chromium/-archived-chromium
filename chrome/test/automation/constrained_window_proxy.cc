// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  int title_size_response = -1;

  sender_->Send(new AutomationMsg_ConstrainedTitle(0, handle_,
                                                   &title_size_response,
                                                   title));
  return title_size_response >= 0;
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

  bool result = false;

  sender_->SendWithTimeout(new AutomationMsg_ConstrainedWindowBounds(
      0, handle_, &result, bounds), timeout_ms, NULL);

  return result;
}
