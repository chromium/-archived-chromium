// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include "base/logging.h"

namespace views {

FocusManager* View::GetFocusManager() {
  NOTIMPLEMENTED();
  return NULL;
}

void View::DoDrag(const MouseEvent& e, int press_x, int press_y) {
  NOTIMPLEMENTED();
}

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  NOTIMPLEMENTED();
  return NULL;
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
