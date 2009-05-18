// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util.h"
#include "views/widget/root_view.h"

BrowserBubble::BrowserBubble(views::View* view, views::Widget* frame,
                             const gfx::Point& origin)
    : frame_(frame),
      view_(view),
      visible_(false),
      delegate_(NULL) {
  gfx::Size size = view->GetPreferredSize();
  bounds_.SetRect(origin.x(), origin.y(), size.width(), size.height());
  InitPopup();
}

BrowserBubble::~BrowserBubble() {
  DestroyPopup();
}

void BrowserBubble::BrowserWindowMoved() {
  if (delegate_)
    delegate_->BubbleBrowserWindowMoved(this);
  else
    Hide();
}

void BrowserBubble::BrowserWindowClosed() {
  if (delegate_)
    delegate_->BubbleBrowserWindowClosed(this);
  else
    Hide();
}

void BrowserBubble::SetBounds(int x, int y, int w, int h) {
  // If the UI layout is RTL, we need to mirror the position of the bubble
  // relative to the parent.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    gfx::Rect frame_bounds;
    frame_->GetBounds(&frame_bounds, false);
    x = frame_bounds.width() - x - w;
  }
  bounds_.SetRect(x, y, w, h);
  Reposition();
}

void BrowserBubble::Reposition() {
  gfx::Point top_left;
  views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);
  MovePopup(top_left.x() + bounds_.x(),
            top_left.y() + bounds_.y(),
            bounds_.width(),
            bounds_.height());
}
