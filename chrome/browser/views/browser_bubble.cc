// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

BrowserBubble::BrowserBubble(views::View* view, views::Widget* frame,
                             const gfx::Point& origin)
    : frame_(frame),
      view_(view),
      visible_(false),
      delegate_(NULL),
      attached_(false) {
  frame_native_view_ = frame_->GetNativeView();
  gfx::Size size = view->GetPreferredSize();
  bounds_.SetRect(origin.x(), origin.y(), size.width(), size.height());
  InitPopup();
}

BrowserBubble::~BrowserBubble() {
  DCHECK(!attached_);
  popup_->CloseNow();
  // Don't call DetachFromBrowser from here.  It needs to talk to the
  // BrowserView to deregister itself, and if BrowserBubble is owned
  // by a child of BrowserView, then it's possible that this stack frame
  // is a descendant of BrowserView's destructor, which leads to problems.
  // In that case, Detach doesn't need to get called anyway since BrowserView
  // will do the necessary cleanup.
}

void BrowserBubble::DetachFromBrowser() {
  DCHECK(attached_);
  if (!attached_)
    return;
  attached_ = false;
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      frame_->GetWindow()->GetNativeWindow());
  if (browser_view)
    browser_view->DetachBrowserBubble(this);
}

void BrowserBubble::AttachToBrowser() {
  DCHECK(!attached_);
  if (attached_)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      frame_->GetWindow()->GetNativeWindow());
  DCHECK(browser_view);
  if (browser_view) {
    browser_view->AttachBrowserBubble(this);
    attached_ = true;
  }
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
  // If the UI layout is RTL, we don't need to mirror coordinates, since
  // View logic will do that for us.
  bounds_.SetRect(x, y, w, h);
  Reposition();
}

void BrowserBubble::MoveTo(int x, int y) {
  SetBounds(x, y, bounds_.width(), bounds_.height());
}

void BrowserBubble::Reposition() {
  gfx::Point top_left;
  views::View::ConvertPointToScreen(frame_->GetRootView(), &top_left);
  MovePopup(top_left.x() + bounds_.x(),
            top_left.y() + bounds_.y(),
            bounds_.width(),
            bounds_.height());
}

void BrowserBubble::ResizeToView() {
  gfx::Size size = view_->GetPreferredSize();
  SetBounds(bounds_.x(), bounds_.y(), size.width(), size.height());
}
