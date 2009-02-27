// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"
#include "chrome/views/widget.h"

namespace views {

const int NonClientView::kFrameShadowThickness = 1;
const int NonClientView::kClientEdgeThickness = 1;

////////////////////////////////////////////////////////////////////////////////
// NonClientView, public:

NonClientView::NonClientView() : paint_as_active_(false) {
}

NonClientView::~NonClientView() {
}

bool NonClientView::CanClose() const {
  return client_view_->CanClose();
}

void NonClientView::WindowClosing() {
  client_view_->WindowClosing();
}

gfx::Rect NonClientView::CalculateClientAreaBounds(int width,
                                                   int height) const {
  return gfx::Rect();
}

gfx::Size NonClientView::CalculateWindowSizeForClientSize(int width,
                                                          int height) const {
  return gfx::Size();
}

gfx::Point NonClientView::GetSystemMenuPoint() const {
  CPoint temp(0, -kFrameShadowThickness);
  MapWindowPoints(GetWidget()->GetHWND(), HWND_DESKTOP, &temp, 1);
  return gfx::Point(temp);
}

int NonClientView::NonClientHitTest(const gfx::Point& point) {
  return client_view_->NonClientHitTest(point);
}

void NonClientView::GetWindowMask(const gfx::Size& size,
                                  gfx::Path* window_mask) {
}

void NonClientView::EnableClose(bool enable) {
}

void NonClientView::ResetWindowControls() {
}


////////////////////////////////////////////////////////////////////////////////
// NonClientView, View overrides:

gfx::Size NonClientView::GetPreferredSize() {
  return client_view_->GetPreferredSize();
}

void NonClientView::Layout() {
  client_view_->SetBounds(0, 0, width(), height());
}

void NonClientView::ViewHierarchyChanged(bool is_add, View* parent,
                                         View* child) {
  // Add our Client View as we are added to the Widget so that if we are
  // subsequently resized all the parent-child relationships are established.
  if (is_add && GetWidget() && child == this)
    AddChildView(client_view_);
}

////////////////////////////////////////////////////////////////////////////////
// NonClientView, protected:

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

