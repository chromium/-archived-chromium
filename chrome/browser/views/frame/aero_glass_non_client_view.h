// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_

#include "chrome/browser/views/frame/aero_glass_frame.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/button.h"

class BrowserView;
class AeroGlassWindowResources;

class AeroGlassNonClientView : public views::NonClientView {
 public:
  // Constructs a non-client view for an AeroGlassFrame.
  AeroGlassNonClientView(AeroGlassFrame* frame, BrowserView* browser_view);
  virtual ~AeroGlassNonClientView();

  // Retrieve the bounds for the specified |tabstrip|, in the coordinate system
  // of the non-client view (which whould be window coordinates).
  gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip);

 protected:
  // Overridden from views::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) { }
  virtual void EnableClose(bool enable) { }
  virtual void ResetWindowControls() { }

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

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

  // Paint various sub-components of this view.
  void PaintDistributorLogo(ChromeCanvas* canvas);
  void PaintToolbarBackground(ChromeCanvas* canvas);
  void PaintOTRAvatar(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutDistributorLogo();
  void LayoutOTRAvatar();
  void LayoutClientView();
 
  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // The layout rect of the OTR avatar.
  gfx::Rect otr_avatar_bounds_;

  // The frame that hosts this view.
  AeroGlassFrame* frame_;

  // The BrowserView that we contain.
  BrowserView* browser_view_;

  static void InitClass();
  static SkBitmap distributor_logo_;
  static AeroGlassWindowResources* resources_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassNonClientView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_

