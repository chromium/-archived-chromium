// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/single_split_view.h"

#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/background.h"
#include "skia/ext/skia_utils_win.h"

namespace views {

// Size of the divider in pixels.
static const int kDividerSize = 4;

SingleSplitView::SingleSplitView(View* leading, View* trailing)
    : divider_x_(-1) {
  AddChildView(leading);
  AddChildView(trailing);
  set_background(
      views::Background::CreateSolidBackground(
          skia::COLORREFToSkColor(GetSysColor(COLOR_3DFACE))));
}

void SingleSplitView::Layout() {
  if (GetChildViewCount() != 2)
    return;

  View* leading = GetChildViewAt(0);
  View* trailing = GetChildViewAt(1);
  if (divider_x_ < 0)
    divider_x_ = (width() - kDividerSize) / 2;
  else
    divider_x_ = std::min(divider_x_, width() - kDividerSize);
  leading->SetBounds(0, 0, divider_x_, height());
  trailing->SetBounds(divider_x_ + kDividerSize, 0,
                      width() - divider_x_ - kDividerSize, height());

  SchedulePaint();

  // Invoke super's implementation so that the children are layed out.
  View::Layout();
}

gfx::Size SingleSplitView::GetPreferredSize() {
  int width = 0;
  int height = 0;
  for (int i = 0; i < 2 && i < GetChildViewCount(); ++i) {
    View* view = GetChildViewAt(i);
    gfx::Size pref = view->GetPreferredSize();
    width += pref.width();
    height = std::max(height, pref.height());
  }
  width += kDividerSize;
  return gfx::Size(width, height);
}

HCURSOR SingleSplitView::GetCursorForPoint(Event::EventType event_type,
                                           int x,
                                           int y) {
  if (IsPointInDivider(x)) {
    static HCURSOR resize_cursor = LoadCursor(NULL, IDC_SIZEWE);
    return resize_cursor;
  }
  return NULL;
}

bool SingleSplitView::OnMousePressed(const MouseEvent& event) {
  if (!IsPointInDivider(event.x()))
    return false;
  drag_info_.initial_mouse_x = event.x();
  drag_info_.initial_divider_x = divider_x_;
  return true;
}

bool SingleSplitView::OnMouseDragged(const MouseEvent& event) {
  if (GetChildViewCount() < 2)
    return false;

  int delta_x = event.x() - drag_info_.initial_mouse_x;
  if (UILayoutIsRightToLeft())
    delta_x *= -1;
  // Honor the minimum size when resizing.
  int new_width = std::max(GetChildViewAt(0)->GetMinimumSize().width(),
                           drag_info_.initial_divider_x + delta_x);

  // And don't let the view get bigger than our width.
  new_width = std::min(width() - kDividerSize, new_width);

  if (new_width != divider_x_) {
    set_divider_x(new_width);
    Layout();
  }
  return true;
}

void SingleSplitView::OnMouseReleased(const MouseEvent& event, bool canceled) {
  if (GetChildViewCount() < 2)
    return;

  if (canceled && drag_info_.initial_divider_x != divider_x_) {
    set_divider_x(drag_info_.initial_divider_x);
    Layout();
  }
}

bool SingleSplitView::IsPointInDivider(int x) {
  if (GetChildViewCount() < 2)
    return false;

  int divider_relative_x =
      x - GetChildViewAt(UILayoutIsRightToLeft() ? 1 : 0)->width();
  return (divider_relative_x >= 0 && divider_relative_x < kDividerSize);
}

}  // namespace views
