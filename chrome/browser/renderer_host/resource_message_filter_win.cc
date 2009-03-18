// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                            gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  GetWindowRect(window, &window_rect);
  *rect = window_rect;
}

void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                                gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  HWND root_window = ::GetAncestor(window, GA_ROOT);
  GetWindowRect(root_window, &window_rect);
  *rect = window_rect;
}
