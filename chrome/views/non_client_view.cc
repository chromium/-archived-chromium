// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"

#include "chrome/common/win_util.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"
#include "chrome/views/window.h"

namespace views {

const int NonClientFrameView::kFrameShadowThickness = 1;
const int NonClientFrameView::kClientEdgeThickness = 1;

// The frame view and the client view are always at these specific indices,
// because the RootView message dispatch sends messages to items higher in the
// z-order first and we always want the client view to have first crack at
// handling mouse messages.
static const int kFrameViewIndex = 0;
static const int kClientViewIndex = 1;

////////////////////////////////////////////////////////////////////////////////
// NonClientView, public:

NonClientView::NonClientView(Window* frame)
    : frame_(frame),
      client_view_(NULL),
      use_native_frame_(win_util::ShouldUseVistaFrame()) {
}

NonClientView::~NonClientView() {
  // This value may have been reset before the window hierarchy shuts down,
  // so we need to manually remove it.
  RemoveChildView(frame_view_.get());
}

void NonClientView::SetFrameView(NonClientFrameView* frame_view) {
  // See comment in header about ownership.
  frame_view->SetParentOwned(false);
  if (frame_view_.get())
    RemoveChildView(frame_view_.get());
  frame_view_.reset(frame_view);
  if (GetParent())
    AddChildView(kFrameViewIndex, frame_view_.get());
}

bool NonClientView::CanClose() const {
  return client_view_->CanClose();
}

void NonClientView::WindowClosing() {
  client_view_->WindowClosing();
}

void NonClientView::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
  SetFrameView(frame_->CreateFrameViewForWindow());
  GetRootView()->ThemeChanged();
  Layout();
  SchedulePaint();
  frame_->UpdateFrameAfterFrameChange();
}

bool NonClientView::UseNativeFrame() const {
  // The frame view may always require a custom frame, e.g. Constrained Windows.
  bool always_use_custom_frame =
      frame_view_.get() && frame_view_->AlwaysUseCustomFrame();
  return !always_use_custom_frame && use_native_frame_;
}

void NonClientView::DisableInactiveRendering(bool disable) {
  frame_view_->DisableInactiveRendering(disable);
}

gfx::Rect NonClientView::GetWindowBoundsForClientBounds(
    const gfx::Rect client_bounds) const {
  return frame_view_->GetWindowBoundsForClientBounds(client_bounds);
}

gfx::Point NonClientView::GetSystemMenuPoint() const {
  return frame_view_->GetSystemMenuPoint();
}

int NonClientView::NonClientHitTest(const gfx::Point& point) {
  // Sanity check.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  // The ClientView gets first crack, since it overlays the NonClientFrameView
  // in the display stack.
  int frame_component = client_view_->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Finally ask the NonClientFrameView. It's at the back of the display stack
  // so it gets asked last.
  return frame_view_->NonClientHitTest(point);
}

void NonClientView::GetWindowMask(const gfx::Size& size,
                                  gfx::Path* window_mask) {
  frame_view_->GetWindowMask(size, window_mask);
}

void NonClientView::EnableClose(bool enable) {
  frame_view_->EnableClose(enable);
}

void NonClientView::ResetWindowControls() {
  frame_view_->ResetWindowControls();
}

////////////////////////////////////////////////////////////////////////////////
// NonClientView, View overrides:

gfx::Size NonClientView::GetPreferredSize() {
  gfx::Rect client_bounds(gfx::Point(), client_view_->GetPreferredSize());
  return GetWindowBoundsForClientBounds(client_bounds).size();
}

void NonClientView::Layout() {
  // First layout the NonClientFrameView, which determines the size of the
  // ClientView...
  frame_view_->SetBounds(0, 0, width(), height());

  // We need to manually call Layout here because layout for the frame view can
  // change independently of the bounds changing - e.g. after the initial
  // display of the window the metrics of the native window controls can change,
  // which does not change the bounds of the window but requires a re-layout to
  // trigger a repaint. We override DidChangeBounds for the NonClientFrameView
  // to do nothing so that SetBounds above doesn't cause Layout to be called
  // twice.
  frame_view_->Layout();

  // Then layout the ClientView, using those bounds.
  client_view_->SetBounds(frame_view_->GetBoundsForClientView());

  // We need to manually call Layout on the ClientView as well for the same
  // reason as above.
  client_view_->Layout();
}

void NonClientView::ViewHierarchyChanged(bool is_add, View* parent,
                                         View* child) {
  // Add our two child views here as we are added to the Widget so that if we
  // are subsequently resized all the parent-child relationships are
  // established.
  if (is_add && GetWidget() && child == this) {
    AddChildView(kFrameViewIndex, frame_view_.get());
    AddChildView(kClientViewIndex, client_view_);
  }
}

views::View* NonClientView::GetViewForPoint(const gfx::Point& point) {
  return GetViewForPoint(point, false);
}

views::View* NonClientView::GetViewForPoint(const gfx::Point& point,
                                            bool can_create_floating) {
  // Because of the z-ordering of our child views (the client view is positioned
  // over the non-client frame view, if the client view ever overlaps the frame
  // view visually (as it does for the browser window), then it will eat mouse
  // events for the window controls. We override this method here so that we can
  // detect this condition and re-route the events to the non-client frame view.
  // The assumption is that the frame view's implementation of HitTest will only
  // return true for area not occupied by the client view.
  gfx::Point point_in_child_coords(point);
  View::ConvertPointToView(this, frame_view_.get(), &point_in_child_coords);
  if (frame_view_->HitTest(point_in_child_coords))
    return frame_view_->GetViewForPoint(point);

  return View::GetViewForPoint(point, can_create_floating);
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView, View overrides:

bool NonClientFrameView::HitTest(const gfx::Point& l) const {
  // For the default case, we assume the non-client frame view never overlaps
  // the client view.
  return !GetWidget()->AsWindow()->GetClientView()->bounds().Contains(l);
}

void NonClientFrameView::DidChangeBounds(const gfx::Rect& previous,
                                         const gfx::Rect& current) {
  // Overridden to do nothing. The NonClientView manually calls Layout on the
  // FrameView when it is itself laid out, see comment in NonClientView::Layout.
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView, protected:

int NonClientFrameView::GetHTComponentForFrame(const gfx::Point& point,
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
