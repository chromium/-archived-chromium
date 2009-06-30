// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/browser_bubble.h"

#include "app/l10n_util_win.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

void BrowserBubble::InitPopup() {
  gfx::NativeWindow native_window = frame_->GetWindow()->GetNativeWindow();
  views::WidgetWin* pop = new views::WidgetWin();
  pop->set_delete_on_destroy(false);
  pop->set_window_style(WS_POPUP);
#if 0
  // TODO(erikkay) Layered windows don't draw child windows.
  // Apparently there's some tricks you can do to handle that.
  // Do the research so we can use this.
  pop->set_window_ex_style(WS_EX_LAYERED |
                           l10n_util::GetExtendedTooltipStyles());
  pop->SetOpacity(0xFF);
#endif
  // A focus manager is necessary if you want to be able to handle various
  // mouse events properly.
  pop->Init(frame_native_view_, bounds_);
  pop->SetContentsView(view_);
  popup_.reset(pop);
  Reposition();

  AttachToBrowser();
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
