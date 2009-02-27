// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_NON_CLIENT_VIEW_H_
#define CHROME_VIEWS_NON_CLIENT_VIEW_H_

#include "chrome/views/view.h"
#include "chrome/views/client_view.h"

namespace gfx {
class Path;
}

namespace views {

///////////////////////////////////////////////////////////////////////////////
// NonClientView
//
//  An object implementing the NonClientView interface is a View that provides
//  the "non-client" areas of a window. This is the area that typically
//  encompasses the window frame - title bar, sizing borders and window
//  controls. This interface provides methods that allow a specific
//  presentation to define non-client areas for windows hit testing, the shape
//  of the window, and other window-related information.
//
class NonClientView : public View {
 public:
  NonClientView();
  virtual ~NonClientView();

  // Various edges of the frame border have a 1 px shadow along their edges; in
  // a few cases we shift elements based on this amount for visual appeal.
  static const int kFrameShadowThickness;
  // In restored mode, we draw a 1 px edge around the content area inside the
  // frame border.
  static const int kClientEdgeThickness;

  // Returns true if the ClientView determines that the containing window can be
  // closed, false otherwise.
  bool CanClose() const;

  // Called by the containing Window when it is closed.
  void WindowClosing();

  // Calculates the bounds of the client area of the window assuming the
  // window is sized to |width| and |height|.
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // Calculates the size of window required to display a client area of the
  // specified width and height.
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;

  // Returns the point, in screen coordinates, where the system menu should
  // be shown so it shows up anchored to the system menu icon.
  virtual gfx::Point GetSystemMenuPoint() const;

  // Determines the windows HT* code when the mouse cursor is at the
  // specified point, in window coordinates.
  virtual int NonClientHitTest(const gfx::Point& point);

  // Returns a mask to be used to clip the top level window for the given
  // size. This is used to create the non-rectangular window shape.
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask);

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  virtual void EnableClose(bool enable);

  // Tells the window controls as rendered by the NonClientView to reset
  // themselves to a normal state. This happens in situations where the
  // containing window does not receive a normal sequences of messages that
  // would lead to the controls returning to this normal state naturally, e.g.
  // when the window is maximized, minimized or restored.
  virtual void ResetWindowControls();

  // Prevents the non-client view from rendering as inactive when called with
  // |disable| true, until called with false.
  void set_paint_as_active(bool paint_as_active) { 
    paint_as_active_ = paint_as_active;
  }

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
  // A ClientView object or subclass, responsible for sizing the contents view
  // of the window, hit testing and perhaps other tasks depending on the
  // implementation.
  ClientView* client_view_;

  // True when the non-client view should always be rendered as if the window
  // were active, regardless of whether or not the top level window actually
  // is active.
  bool paint_as_active_;
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_NON_CLIENT_VIEW_H_

