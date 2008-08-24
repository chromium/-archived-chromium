// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_

#include "chrome/browser/views/frame/aero_glass_frame.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/button.h"

class WindowResources;

class AeroGlassNonClientView : public ChromeViews::NonClientView {
 public:
  // Constructs a non-client view for an AeroGlassFrame.
  explicit AeroGlassNonClientView(AeroGlassFrame* frame);
  virtual ~AeroGlassNonClientView();

  gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip);

 protected:
  // Overridden from ChromeViews::NonClientView:
  virtual gfx::Rect CalculateClientAreaBounds(int width, int height) const;
  virtual gfx::Size CalculateWindowSizeForClientSize(int width,
                                                     int height) const;
  virtual CPoint GetSystemMenuPoint() const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);

  // Overridden from ChromeViews::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Returns the height of the non-client area at the top of the window (the
  // title bar, etc).
  int CalculateNonClientTopHeight() const;

  // Paint various sub-components of this view.
  void PaintDistributorLogo(ChromeCanvas* canvas);
  void PaintToolbarBackground(ChromeCanvas* canvas);
  void PaintClientEdge(ChromeCanvas* canvas);

  // Layout various sub-components of this view.
  void LayoutDistributorLogo();
  void LayoutClientView();
 
  // The layout rect of the distributor logo, if visible.
  gfx::Rect logo_bounds_;

  // The frame that hosts this view.
  AeroGlassFrame* frame_;

  static void InitClass();
  static SkBitmap distributor_logo_;
  static WindowResources* resources_;

  DISALLOW_EVIL_CONSTRUCTORS(AeroGlassNonClientView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_FRAME_AERO_GLASS_NON_CLIENT_VIEW_H_

