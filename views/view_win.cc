// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/os_exchange_data.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "views/accessibility/view_accessibility_wrapper.h"
#include "views/border.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace views {

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

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  if (accessibility_.get() == NULL) {
    accessibility_.reset(new ViewAccessibilityWrapper(this));
  }
  return accessibility_.get();
}

void View::Focus() {
  // Set the native focus to the root view window so it receives the keyboard
  // messages.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->FocusNativeView(GetRootView()->GetWidget()->GetNativeView());
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
