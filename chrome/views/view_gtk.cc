// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/view.h"

#include "base/logging.h"

namespace views {

FocusManager* View::GetFocusManager() {
  NOTIMPLEMENTED();
  return NULL;
}

void View::DoDrag(const MouseEvent& e, int press_x, int press_y) {
  NOTIMPLEMENTED();
}

AccessibleWrapper* View::GetAccessibleWrapper() {
  NOTIMPLEMENTED();
  return NULL;
}

bool View::HitTest(const gfx::Point& l) const {
  if (l.x() >= 0 && l.x() < static_cast<int>(width()) &&
      l.y() >= 0 && l.y() < static_cast<int>(height())) {
    if (HasHitTestMask()) {
      // TODO(port): port the windows hit test code here. Once that's factored
      // out, we can probably move View::HitTest back into views.cc.
      NOTIMPLEMENTED();
    }
    // No mask, but inside our bounds.
    return true;
  }
  // Outside our bounds.
  return false;
}

void View::Focus() {
  NOTIMPLEMENTED();
}

int View::GetHorizontalDragThreshold() {
  static int threshold = -1;
  NOTIMPLEMENTED();
  return threshold;
}

int View::GetVerticalDragThreshold() {
  static int threshold = -1;
  NOTIMPLEMENTED();
  return threshold;
}

}  // namespace views
