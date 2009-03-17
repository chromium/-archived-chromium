// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
#define CHROME_VIEWS_WINDOW_NON_CLIENT_VIEW_H_

#include "base/task.h"
#include "chrome/views/view.h"
#include "chrome/views/window/client_view.h"

namespace gfx {
class Path;
}

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameView
//
//  An object that subclasses NonClientFrameView is a View that renders and
//  responds to events within the frame portions of the non-client area of a
//  window. This view does _not_ contain the ClientView, but rather is a sibling
//  of it.
class NonClientFrameView : public View {
 public:
  // Various edges of the frame border have a 1 px shadow along their edges; in
  // a few cases we shift elements based on this amount for visual appeal.
  static const int kFrameShadowThickness;
  // In restored mode, we draw a 1 px edge around the content area inside the
  // frame border.
  static const int kClientEdgeThickness;

  void DisableInactiveRendering(bool disable) {
    paint_as_active_ = disable;
    if (!paint_as_active_)
      SchedulePaint();
  }

  // Returns the bounds (in this View's parent's coordinates) that the client
  // view should be laid out within.
  virtual gfx::Rect GetBoundsForClientView() const = 0;

  // Returns true if this FrameView should always use the custom frame,
  // regardless of the system settings. An example is the Constrained Window,
  // which is a child window and must always provide its own frame.
  virtual bool AlwaysUseCustomFrame() const { return false; }

  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const = 0;
  virtual gfx::Point GetSystemMenuPoint() const = 0;
  virtual int NonClientHitTest(const gfx::Point& point) = 0;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) = 0;
  virtual void EnableClose(bool enable) = 0;
  virtual void ResetWindowControls() = 0;

  // Overridden from View:
  virtual bool HitTest(const gfx::Point& l) const;

 protected:
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

  NonClientFrameView() : paint_as_active_(false) {}


  // Helper for non-client view implementations to determine which area of the
  // window border the specified |point| falls within. The other parameters are
  // the size of the sizing edges, and whether or not the window can be
  // resized.
  int GetHTComponentForFrame(const gfx::Point& point,
                             int top_resize_border_height,
                             int resize_border_thickness,
                             int top_resize_corner_height,
                             int resize_corner_width,
                             bool can_resize);

  // Accessor for paint_as_active_.
  bool paint_as_active() const { return paint_as_active_; }

 private:
  // True when the non-client view should always be rendered as if the window
  // were active, regardless of whether or not the top level window actually
  // is active.
  bool paint_as_active_;
};

////////////////////////////////////////////////////////////////////////////////
// NonClientView
//
//  The NonClientView is the logical root of all Views contained within a
//  Window, except for the RootView which is its parent and of which it is the
//  sole child. The NonClientView has two children, the NonClientFrameView which
//  is responsible for painting and responding to events from the non-client
//  portions of the window, and the ClientView, which is responsible for the
//  same for the client area of the window:
//
//  +- views::Window ------------------------------------+
//  | +- views::RootView ------------------------------+ |
//  | | +- views::NonClientView ---------------------+ | |
//  | | | +- views::NonClientView subclass      ---+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the non-client areas of a     >> | | | |
//  | | | | << views::Window.                   >> | | | |
//  | | | |                                        | | | |
//  | | | +----------------------------------------+ | | |
//  | | | +- views::ClientView or subclass --------+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the client areas of a         >> | | | |
//  | | | | << views::Window.                   >> | | | |
//  | | | |                                        | | | |
//  | | | +----------------------------------------+ | | |
//  | | +--------------------------------------------+ | |
//  | +------------------------------------------------+ |
//  +----------------------------------------------------+
//
// The NonClientFrameView and ClientView are siblings because due to theme
// changes the NonClientFrameView may be replaced with different
// implementations (e.g. during the switch from DWM/Aero-Glass to Vista Basic/
// Classic rendering).
//
class NonClientView : public View {
 public:
  explicit NonClientView(Window* frame);
  virtual ~NonClientView();

  // Replaces the current NonClientFrameView (if any) with the specified one.
  void SetFrameView(NonClientFrameView* frame_view);

  // Returns true if the ClientView determines that the containing window can be
  // closed, false otherwise.
  bool CanClose() const;

  // Called by the containing Window when it is closed.
  void WindowClosing();

  // Changes the frame from native to custom depending on the value of
  // |use_native_frame|.
  void SetUseNativeFrame(bool use_native_frame);

  // Returns true if the native window frame should be used, false if the
  // NonClientView provides its own frame implementation.
  bool UseNativeFrame() const;

  // Prevents the window from being rendered as deactivated when |disable| is
  // true, until called with |disable| false. Used when a sub-window is to be
  // shown that shouldn't visually de-activate the window.
  // Subclasses can override this to perform additional actions when this value
  // changes.
  void DisableInactiveRendering(bool disable);

  // Returns the bounds of the window required to display the content area at
  // the specified bounds.
  gfx::Rect GetWindowBoundsForClientBounds(const gfx::Rect client_bounds) const;

  // Returns the point, in screen coordinates, where the system menu should
  // be shown so it shows up anchored to the system menu icon.
  gfx::Point GetSystemMenuPoint() const;

  // Determines the windows HT* code when the mouse cursor is at the
  // specified point, in window coordinates.
  int NonClientHitTest(const gfx::Point& point);

  // Returns a mask to be used to clip the top level window for the given
  // size. This is used to create the non-rectangular window shape.
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  void EnableClose(bool enable);

  // Tells the window controls as rendered by the NonClientView to reset
  // themselves to a normal state. This happens in situations where the
  // containing window does not receive a normal sequences of messages that
  // would lead to the controls returning to this normal state naturally, e.g.
  // when the window is maximized, minimized or restored.
  void ResetWindowControls();

  // Get/Set client_view property.
  ClientView* client_view() const { return client_view_; }
  void set_client_view(ClientView* client_view) {
    client_view_ = client_view;
  }

  // NonClientView, View overrides:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 protected:
  // NonClientView, View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual views::View* GetViewForPoint(const gfx::Point& point);
  virtual views::View* GetViewForPoint(const gfx::Point& point,
                                       bool can_create_floating);

 private:
  // The frame that hosts this NonClientView.
  Window* frame_;

  // A ClientView object or subclass, responsible for sizing the contents view
  // of the window, hit testing and perhaps other tasks depending on the
  // implementation.
  ClientView* client_view_;

  // The NonClientFrameView that renders the non-client portions of the window.
  // This object is not owned by the view hierarchy because it can be replaced
  // dynamically as the system settings change.
  scoped_ptr<NonClientFrameView> frame_view_;

  // Whether or not we should use the native frame.
  bool use_native_frame_;

  DISALLOW_COPY_AND_ASSIGN(NonClientView);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_WINDOW_NON_CLIENT_VIEW_H_
