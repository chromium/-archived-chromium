// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                            IPC::Message* reply_msg) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  GetWindowRect(window, &window_rect);
  gfx::Rect rect(window_rect);

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                                IPC::Message* reply_msg) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  HWND root_window = ::GetAncestor(window, GA_ROOT);
  GetWindowRect(root_window, &window_rect);
  gfx::Rect rect(window_rect);

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}
