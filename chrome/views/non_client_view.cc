// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/non_client_view.h"

namespace ChromeViews {

int NonClientView::GetHTComponentForFrame(const gfx::Point& point,
                                          int resize_area_size,
                                          int resize_area_corner_size,
                                          int top_resize_area_size,
                                          bool can_resize) {
  int width = GetWidth();
  int height = GetHeight();
  int component = HTNOWHERE;
  if (point.x() < resize_area_size) {
    if (point.y() < resize_area_corner_size) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height - resize_area_corner_size)) {
      component = HTBOTTOMLEFT;
    } else {
      component = HTLEFT;
    }
  } else if (point.x() < resize_area_corner_size) {
    if (point.y() < top_resize_area_size) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height - resize_area_size)) {
      component = HTBOTTOMLEFT;
    }
  } else if (point.x() >= (width - resize_area_size)) {
    if (point.y() < resize_area_corner_size) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height - resize_area_corner_size)) {
      component = HTBOTTOMRIGHT;
    } else if (point.x() >= (width - resize_area_size)) {
      component = HTRIGHT;
    }
  } else if (point.x() >= (width - resize_area_corner_size)) {
    if (point.y() < top_resize_area_size) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height - resize_area_size)) {
      component = HTBOTTOMRIGHT;
    }
  } else if (point.y() < top_resize_area_size) {
    component = HTTOP;
  } else if (point.y() >= (height - resize_area_size)) {
    component = HTBOTTOM;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  if (component != HTNOWHERE)
    return can_resize ? component : HTBORDER;
  return HTNOWHERE;
}

}  // namespace ChromeViews
