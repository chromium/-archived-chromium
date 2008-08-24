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

