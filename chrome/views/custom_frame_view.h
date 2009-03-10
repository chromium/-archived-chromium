// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CUSTOM_FRAME_VIEW_H_
#define CHROME_VIEWS_CUSTOM_FRAME_VIEW_H_

#include "chrome/views/button.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/window.h"
#include "chrome/views/window_resources.h"

namespace gfx{
class Size;
class Path;
class Point;
}
class ChromeCanvas;
class ChromeFont;

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// CustomFrameView
//
//  A ChromeView that provides the non client frame for Windows. This means
//  rendering the non-standard window caption, border, and controls.
//
////////////////////////////////////////////////////////////////////////////////
class CustomFrameView : public NonClientFrameView,
                        public BaseButton::ButtonListener {
 public:
  explicit CustomFrameView(Window* frame);
  virtual ~CustomFrameView();

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual gfx::Point GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

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
    return frame_->is_active() || paint_as_active() ? active_resources_
                                                    : inactive_resources_;
  }

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;

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
  Window* frame_;

  // Initialize various static resources.
  static void InitClass();
  static WindowResources* active_resources_;
  static WindowResources* inactive_resources_;
  static ChromeFont title_font_;

  DISALLOW_EVIL_CONSTRUCTORS(CustomFrameView);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CUSTOM_FRAME_VIEW_H_
