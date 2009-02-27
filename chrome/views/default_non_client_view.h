// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_DEFAULT_NON_CLIENT_VIEW_H_
#define CHROME_VIEWS_DEFAULT_NON_CLIENT_VIEW_H_

#include "chrome/views/button.h"
#include "chrome/views/custom_frame_window.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/window_resources.h"

namespace gfx{
class Size;
class Path;
class Point;
}
class ChromeCanvas;

namespace views {

class ClientView;

///////////////////////////////////////////////////////////////////////////////
//
// DefaultNonClientView
//
//  A ChromeView that provides the "frame" for CustomFrameWindows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class DefaultNonClientView : public NonClientView,
                             public BaseButton::ButtonListener {
 public:
  explicit DefaultNonClientView(CustomFrameWindow* container);
  virtual ~DefaultNonClientView();

  // Overridden from views::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual gfx::Point GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // BaseButton::ButtonListener implementation:
  virtual void ButtonPressed(BaseButton* sender);

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // A bottom border, and, in restored mode, a client edge are drawn at the
  // bottom of the titlebar.  This returns the total height drawn.
  int BottomEdgeThicknessWithinNonClientHeight() const;

  // Calculates multiple values related to title layout.  Returns the height of
  // the entire titlebar including any connected client edge.
  int TitleCoordinates(int* title_top_spacing,
                       int* title_thickness) const;

  // Paint various sub-components of this view.
  void PaintRestoredFrameBorder(ChromeCanvas* canvas);
  void PaintMaximizedFrameBorder(ChromeCanvas* canvas);
  void PaintTitleBar(ChromeCanvas* canvas);
  void PaintRestoredClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutClientView();

  // Returns the resource collection to be used when rendering the window.
  WindowResources* resources() const {
    return container_->is_active() || paint_as_active() ? active_resources_
                                                        : inactive_resources_;
  }

  // The View that provides the background for the window, and optionally
  // dialog buttons. Note: the non-client view does _not_ own this view, the
  // container does.
  ClientView* client_view_;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Window controls.
  Button* close_button_;
  Button* restore_button_;
  Button* maximize_button_;
  Button* minimize_button_;
  Button* system_menu_button_;  // Uses the window icon if visible.
  bool should_show_minmax_buttons_;

  // The window that owns this view.
  CustomFrameWindow* container_;

  // Initialize various static resources.
  static void InitClass();
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultNonClientView);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_DEFAULT_NON_CLIENT_VIEW_H_
