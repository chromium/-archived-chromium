// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util_win.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

void BrowserBubble::InitPopup() {
  gfx::NativeView native_view = frame_->GetNativeView();
  gfx::NativeWindow native_window = frame_->GetWindow()->GetNativeWindow();
  views::WidgetWin* pop = new views::WidgetWin();
  pop->set_delete_on_destroy(false);
  pop->set_window_style(WS_POPUP);
  pop->set_window_ex_style(WS_EX_LAYERED |
                           WS_EX_TOOLWINDOW |
                           l10n_util::GetExtendedTooltipStyles());
  pop->SetOpacity(0xFF);
  pop->Init(native_view, bounds_, false);
  pop->SetContentsView(view_);
  popup_.reset(pop);
  Reposition();

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(native_window);
  DCHECK(browser_view);
  if (browser_view)
    browser_view->AttachBrowserBubble(this);
}

void BrowserBubble::DestroyPopup() {
  gfx::NativeWindow native_window = frame_->GetWindow()->GetNativeWindow();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(native_window);
  if (browser_view)
    browser_view->DetachBrowserBubble(this);
}

void BrowserBubble::MovePopup(int x, int y, int w, int h) {
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_.get());
  pop->MoveWindow(x, y, w, h);
}

void BrowserBubble::Show() {
  if (visible_)
    return;
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_.get());
  pop->Show();
  visible_ = true;
}

void BrowserBubble::Hide() {
  if (!visible_)
    return;
  views::WidgetWin* pop = static_cast<views::WidgetWin*>(popup_.get());
  pop->Hide();
  visible_ = false;
}
