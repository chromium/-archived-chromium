// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_drop_info.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "views/event.h"
#include "views/view_constants.h"

BookmarkDropInfo::BookmarkDropInfo(gfx::NativeWindow wnd, int top_margin)
    : source_operations_(0),
      is_control_down_(false),
      last_y_(0),
      drop_operation_(0),
      wnd_(wnd),
      top_margin_(top_margin),
      scroll_up_(false) {
}

void BookmarkDropInfo::Update(const views::DropTargetEvent& event) {
  source_operations_ = event.GetSourceOperations();
  is_control_down_ = event.IsControlDown();
  last_y_ = event.y();

#if defined(OS_WIN)
  RECT client_rect;
  GetClientRect(wnd_, &client_rect);
  bool scroll_down = (last_y_ >= client_rect.bottom - views::kAutoscrollSize);
#else
  // TODO(port): Get the dimensions of the appropriate view/widget.
  NOTIMPLEMENTED();
  bool scroll_down = false;
#endif
  scroll_up_ = (last_y_ <= top_margin_ + views::kAutoscrollSize);
  if (scroll_up_ || scroll_down) {
    if (!scroll_timer_.IsRunning()) {
      scroll_timer_.Start(
          base::TimeDelta::FromMilliseconds(views::kAutoscrollRowTimerMS),
          this,
          &BookmarkDropInfo::Scroll);
    }
  } else {
    scroll_timer_.Stop();
  }
}

void BookmarkDropInfo::Scroll() {
#if defined(OS_WIN)
  SendMessage(wnd_, WM_VSCROLL, scroll_up_ ? SB_LINEUP : SB_LINEDOWN, NULL);
  Scrolled();
#else
  // TODO(port): Scroll the appropriate view/widget.
  NOTIMPLEMENTED();
#endif
}
