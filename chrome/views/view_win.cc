// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/view.h"

#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/views/accessibility/accessible_wrapper.h"
#include "chrome/views/border.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"

namespace views {

FocusManager* View::GetFocusManager() {
  Widget* widget = GetWidget();
  if (!widget)
    return NULL;

  HWND hwnd = widget->GetNativeView();
  if (!hwnd)
    return NULL;

  return FocusManager::GetFocusManager(hwnd);
}

void View::DoDrag(const MouseEvent& e, int press_x, int press_y) {
  int drag_operations = GetDragOperations(press_x, press_y);
  if (drag_operations == DragDropTypes::DRAG_NONE)
    return;

  scoped_refptr<OSExchangeData> data = new OSExchangeData;
  WriteDragData(press_x, press_y, data.get());

  // Message the RootView to do the drag and drop. That way if we're removed
  // the RootView can detect it and avoid calling us back.
  RootView* root_view = GetRootView();
  root_view->StartDragForViewFromMouseEvent(this, data, drag_operations);
}

AccessibleWrapper* View::GetAccessibleWrapper() {
  if (accessibility_.get() == NULL) {
    accessibility_.reset(new AccessibleWrapper(this));
  }
  return accessibility_.get();
}

bool View::HitTest(const gfx::Point& l) const {
  if (l.x() >= 0 && l.x() < static_cast<int>(width()) &&
      l.y() >= 0 && l.y() < static_cast<int>(height())) {
    if (HasHitTestMask()) {
      gfx::Path mask;
      GetHitTestMask(&mask);
      ScopedHRGN rgn(mask.CreateHRGN());
      return !!PtInRegion(rgn, l.x(), l.y());
    }
    // No mask, but inside our bounds.
    return true;
  }
  // Outside our bounds.
  return false;
}

HCURSOR View::GetCursorForPoint(Event::EventType event_type, int x, int y) {
  return NULL;
}

void View::Focus() {
  // Set the native focus to the root view window so it receives the keyboard
  // messages.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->FocusHWND(GetRootView()->GetWidget()->GetNativeView());
}

int View::GetHorizontalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CXDRAG) / 2;
  return threshold;
}

int View::GetVerticalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CYDRAG) / 2;
  return threshold;
}

}  // namespace views
