// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"

namespace views {

const int NonClientView::kFrameShadowThickness = 1;
const int NonClientView::kClientEdgeThickness = 1;

int NonClientView::GetHTComponentForFrame(const gfx::Point& point,
                                          int top_resize_border_height,
                                          int resize_border_thickness,
                                          int top_resize_corner_height,
                                          int resize_corner_width,
                                          bool can_resize) {
  // Tricky: In XP, native behavior is to return HTTOPLEFT and HTTOPRIGHT for
  // a |resize_corner_size|-length strip of both the side and top borders, but
  // only to return HTBOTTOMLEFT/HTBOTTOMRIGHT along the bottom border + corner
  // (not the side border).  Vista goes further and doesn't return these on any
  // of the side borders.  We allow callers to match either behavior.
  int component;
  if (point.x() < resize_border_thickness) {
    if (point.y() < top_resize_corner_height)
      component = HTTOPLEFT;
    else if (point.y() >= (height() - resize_border_thickness))
      component = HTBOTTOMLEFT;
    else
      component = HTLEFT;
  } else if (point.x() >= (width() - resize_border_thickness)) {
    if (point.y() < top_resize_corner_height)
      component = HTTOPRIGHT;
    else if (point.y() >= (height() - resize_border_thickness))
      component = HTBOTTOMRIGHT;
    else
      component = HTRIGHT;
  } else if (point.y() < top_resize_border_height) {
    if (point.x() < resize_corner_width)
      component = HTTOPLEFT;
    else if (point.x() >= (width() - resize_corner_width))
      component = HTTOPRIGHT;
    else
      component = HTTOP;
  } else if (point.y() >= (height() - resize_border_thickness)) {
    if (point.x() < resize_corner_width)
      component = HTBOTTOMLEFT;
    else if (point.x() >= (width() - resize_corner_width))
      component = HTBOTTOMRIGHT;
    else
      component = HTBOTTOM;
  } else {
    return HTNOWHERE;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  return can_resize ? component : HTBORDER;
}

}  // namespace views

