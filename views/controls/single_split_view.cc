// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/single_split_view.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include "app/gfx/canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "views/background.h"

namespace views {

// Size of the divider in pixels.
static const int kDividerSize = 4;

SingleSplitView::SingleSplitView(View* leading,
                                 View* trailing,
                                 Orientation orientation)
    : is_horizontal_(orientation == HORIZONTAL_SPLIT),
      divider_offset_(-1),
      resize_leading_on_bounds_change_(true) {
  AddChildView(leading);
  AddChildView(trailing);
#if defined(OS_WIN)
  set_background(
      views::Background::CreateSolidBackground(
          skia::COLORREFToSkColor(GetSysColor(COLOR_3DFACE))));
#endif
}

void SingleSplitView::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  if (resize_leading_on_bounds_change_) {
    if (is_horizontal_)
      divider_offset_ += current.width() - previous.width();
    else
      divider_offset_ += current.height() - previous.height();

    if (divider_offset_ < 0)
      divider_offset_ = kDividerSize;
  }
  View::DidChangeBounds(previous, current);
}

void SingleSplitView::Layout() {
  if (GetChildViewCount() != 2)
    return;

  View* leading = GetChildViewAt(0);
  View* trailing = GetChildViewAt(1);

  if (!leading->IsVisible() && !trailing->IsVisible())
    return;

  if (!trailing->IsVisible()) {
    leading->SetBounds(0, 0, width(), height());
  } else if (!leading->IsVisible()) {
    trailing->SetBounds(0, 0, width(), height());
  } else {
    if (divider_offset_ < 0)
      divider_offset_ = (GetPrimaryAxisSize() - kDividerSize) / 2;
    else
      divider_offset_ = std::min(divider_offset_,
                                 GetPrimaryAxisSize() - kDividerSize);

    if (is_horizontal_) {
      leading->SetBounds(0, 0, divider_offset_, height());
      trailing->SetBounds(divider_offset_ + kDividerSize, 0,
                          width() - divider_offset_ - kDividerSize, height());
    } else {
      leading->SetBounds(0, 0, width(), divider_offset_);
      trailing->SetBounds(0, divider_offset_ + kDividerSize,
                          width(), height() - divider_offset_ - kDividerSize);
    }
  }

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
    if (is_horizontal_) {
      width += pref.width();
      height = std::max(height, pref.height());
    } else {
      width = std::max(width, pref.width());
      height += pref.height();
    }
  }
  if (is_horizontal_)
    width += kDividerSize;
  else
    height += kDividerSize;
  return gfx::Size(width, height);
}

gfx::NativeCursor SingleSplitView::GetCursorForPoint(Event::EventType event_type,
                                                     int x, int y) {
  if (IsPointInDivider(x, y)) {
#if defined(OS_WIN)
    static HCURSOR we_resize_cursor = LoadCursor(NULL, IDC_SIZEWE);
    static HCURSOR ns_resize_cursor = LoadCursor(NULL, IDC_SIZENS);
    return is_horizontal_ ? we_resize_cursor : ns_resize_cursor;
#elif defined(OS_LINUX)
    return gdk_cursor_new(is_horizontal_ ?
                              GDK_SB_H_DOUBLE_ARROW :
                              GDK_SB_V_DOUBLE_ARROW);
#endif
  }
  return NULL;
}

bool SingleSplitView::OnMousePressed(const MouseEvent& event) {
  if (!IsPointInDivider(event.x(), event.y()))
    return false;
  drag_info_.initial_mouse_offset = GetPrimaryAxisSize(event.x(), event.y());
  drag_info_.initial_divider_offset = divider_offset_;
  return true;
}

bool SingleSplitView::OnMouseDragged(const MouseEvent& event) {
  if (GetChildViewCount() < 2)
    return false;

  int delta_offset = GetPrimaryAxisSize(event.x(), event.y()) -
      drag_info_.initial_mouse_offset;
  if (is_horizontal_ && UILayoutIsRightToLeft())
    delta_offset *= -1;
  // Honor the minimum size when resizing.
  gfx::Size min = GetChildViewAt(0)->GetMinimumSize();
  int new_size = std::max(GetPrimaryAxisSize(min.width(), min.height()),
                          drag_info_.initial_divider_offset + delta_offset);

  // And don't let the view get bigger than our width.
  new_size = std::min(GetPrimaryAxisSize() - kDividerSize, new_size);

  if (new_size != divider_offset_) {
    set_divider_offset(new_size);
    Layout();
  }
  return true;
}

void SingleSplitView::OnMouseReleased(const MouseEvent& event, bool canceled) {
  if (GetChildViewCount() < 2)
    return;

  if (canceled && drag_info_.initial_divider_offset != divider_offset_) {
    set_divider_offset(drag_info_.initial_divider_offset);
    Layout();
  }
}

bool SingleSplitView::IsPointInDivider(int x, int y) {
  if (GetChildViewCount() < 2)
    return false;

  if (!GetChildViewAt(0)->IsVisible() || !GetChildViewAt(1)->IsVisible())
    return false;

  int divider_relative_offset;
  if (is_horizontal_) {
    divider_relative_offset =
        x - GetChildViewAt(UILayoutIsRightToLeft() ? 1 : 0)->width();
  } else {
    divider_relative_offset = y - GetChildViewAt(0)->height();
  }
  return (divider_relative_offset >= 0 &&
      divider_relative_offset < kDividerSize);
}

}  // namespace views
