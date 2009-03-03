// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"

#include "chrome/common/win_util.h"
#include "chrome/views/root_view.h"
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

void NonClientView::SystemThemeChanged() {
  // The window may try to paint in SetUseNativeFrame, and as a result it can
  // get into a state where it is very unhappy with itself - rendering black
  // behind the entire client area. This is because for some reason the
  // SkPorterDuff::kClear_mode erase done in the RootView thinks the window is
  // still opaque. So, to work around this we hide the window as soon as we can
  // (now), saving off its placement so it can be properly restored once
  // everything has settled down.
  WINDOWPLACEMENT saved_window_placement;
  saved_window_placement.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(frame_->GetHWND(), &saved_window_placement);
  frame_->Hide();

  SetUseNativeFrame(win_util::ShouldUseVistaFrame());

  // Now that we've updated the frame, we'll want to restore our saved placement
  // since the display should have settled down and we can be properly rendered.
  SetWindowPlacement(frame_->GetHWND(), &saved_window_placement);
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

  // Then layout the ClientView, using those bounds.
  client_view_->SetBounds(frame_view_->GetBoundsForClientView());
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

